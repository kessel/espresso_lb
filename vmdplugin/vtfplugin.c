/*
VMD file reader plugin for:
- VMD structure format (VSF)
- VMD coordinate format (VCF)
- VMD trajectory format (VTF)
*/
#include <molfile_plugin.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

/* TODO:
- handling of gzipped files?
- volumetric/graphics format
- file write support
*/

/***************************************************
 * Data structures
 ***************************************************/
/* Default atom. 
   Used by vtf_parse_atom to initialise new atoms. */
molfile_atom_t default_atom;

/* Plugin data structure to communciate data between the functions. */
typedef struct {
  /* opened file */
  FILE* file;
  /* return code */
  int return_code;

  /* STRUCTURE DATA (used by read_structure) */
  /* atom info */
  int natoms;
  molfile_atom_t *atoms;
  int optflags;

  /* bond info */
  int nbonds;
  int *from;
  int *to;

  /* TIMESTEP DATA (used by get_next_timestep) */
  /* reading mode for the next timestep */
  int next_indexed;
  /* last timestep */
  float A, B, C, alpha, beta, gamma;
  float *coords;
} vtf_data;

/* global variable: contains the line number of the file */
static int vtf_lineno = 0;

/***************************************************
 * Print an error message.
 ***************************************************/
void vtf_error(const char *msg, const char *line) {
  fprintf(stderr, "vtfplugin:%d: error: %s: %-20s\n", 
	  vtf_lineno, msg, line);
}

/***************************************************
 * Read a line
 ***************************************************/

/* Read a whole line from the file. 
   The line may have arbitrary length and continuation lines ending
   with '\' are heeded.
   The function will return a pointer to a buffer or NULL if an
   error occured or eof occurs while no characters have been read.
   The function will set the global variable lineno.
*/
char *vtf_getline(FILE *file) {
  static char *buffer = NULL;
  static int buffer_size = 0;
  char *s;   /* pointer to the place where the line will be read to */
  int bytes_left;	       /* the number of bytes that are left */
  int l;

  if (buffer == NULL) {
    buffer_size = 255;
    buffer = malloc(buffer_size);
    /* TODO: error handling */
  }

  /* Point s to the beginning of buffer. */
  s = buffer;
  bytes_left = buffer_size;

  if (feof(file)) return NULL;
  do {
    /* read a line */
    if (fgets(s, bytes_left, file) == NULL)
      return NULL;

    vtf_lineno++;

    /* if we reached eof, finish */
    if (feof(file)) break;

    /* pos of the last char */
    l = strlen(s);
    if (s[l-1] == '\n') {
      /* last char is a newline */
      /* overwrite the newline */
      s[l-1] = '\0';
      /* if the last char before the newline is not a continuation
	 char, the line is complete */
      if (s[l-2] != '\\') break;
      else {
	/* last char before is continuation char */
	/* position s to the continuation char */
	bytes_left -= l-2;
	s += l-2;
      }
    } else {
      /* last char is not a newline */
      /* enlarge the buffer */
      buffer_size += 255;
      buffer = realloc(buffer, buffer_size);
      /* TODO: error handling */
      /* reposition s */
      l = strlen(buffer);
      s = buffer + l;
      bytes_left += buffer_size - l;
      vtf_lineno--;
    }
  } while (1);

  /* now check the whole string */
  s = buffer;
  
  /* skip all leading whitespace */
  while (isspace(s[0])) s++;

  /* ignore comment lines */
  if (s[0] == '#') return vtf_getline(file);

  l = strlen(s);

  /* handle empty lines */
  if (l == 0) {
    if (feof(file)) return NULL;
    else return vtf_getline(file);
  }

  return s;
}



/***************************************************
 * Parse ATOM
 ***************************************************/
/* Parse atom data from line. 
   Return MOLFILE_SUCCESS, if data was sucessfully parsed, 
   MOLFILE_ERROR if an error occured. */
int vtf_parse_atom(char *line, vtf_data *d) {
  static molfile_atom_t atom;
  static char aid_specifier[255];
  static char keyword[255];
  static char msg[255];
  char *s;
  int n;
  unsigned int from, to, aid;

  atom = default_atom;
  s = line;

  /* save the aid specifier */
  if (sscanf(s, "%255s %n", aid_specifier, &n) < 1) {
    vtf_error("atom specifier is missing", line);
    return MOLFILE_ERROR;
  }
  s += n;

  /* handle the keywords */
  while (sscanf(s, "%255s %n", keyword, &n) == 1) {
    s += n;
    switch (tolower(keyword[0])) {
    case 'n': {
      /* name */
      if (sscanf(s, "%16s %n", &atom.name, &n) < 1) {
	vtf_error("could not get name in atom record", line);
	return MOLFILE_ERROR;
      }
      s += n;
      break;
    }
    case 't': {
      /* type */
      if (sscanf(s, "%16s %n", &atom.type, &n) < 1) {
	vtf_error("could not get type in atom record", line);
	return MOLFILE_ERROR;
      }
      s += n;
      break;
    }
    case 'r': {
      /* resname, resid, radius */
      if (strlen(keyword) == 1 || 
	  strncmp(keyword, "rad", 3) == 0) { 
	/* radius */
	if (sscanf(s, "%f %n", &atom.radius, &n) < 1) {
	  vtf_error("could not get radius in atom record", line);
	  return MOLFILE_ERROR;
	}
	d->optflags |= MOLFILE_RADIUS;
      } else if (strcmp(keyword, "resid") == 0) {
	/* resid */
	if (sscanf(s, "%d %n", &atom.resid, &n) < 1) {
	  vtf_error("could not get resid in atom record", line);
	  return MOLFILE_ERROR;
	}
      } else if (strcmp(keyword, "res") == 0 || 
		 strcmp(keyword, "resname") == 0) {
	/* resname */
	if (sscanf(s, "%8s %n", &atom.resname, &n) < 1) {
	  vtf_error("could not get resname in atom record", line);
	  return MOLFILE_ERROR;
	}
      } else {
	strcpy(msg, "unrecognized keyword in atom record: ");
	strncat(msg, keyword, 200);
	vtf_error(msg, line);
	return MOLFILE_ERROR;
      }
      s += n;
      break;
    }
    case 's': {
      /* segid */
      if (sscanf(s, "%8s %n", &atom.segid, &n) < 1) {
	vtf_error("could not get segid in atom record", line);
	return MOLFILE_ERROR;
      }
      s += n;
      break;
    }
    case 'i': {
      /* insertion */
      if (sscanf(s, "%2s %n", &atom.insertion, &n) < 1) {
	vtf_error("could not get insertion in atom record", line);
	return MOLFILE_ERROR;
      }
      d->optflags |= MOLFILE_INSERTION;
      s += n;
      break;
    }
    case 'c': {
      /* chain, charge */
      if (strlen(keyword) == 1 || 
	  strcmp(keyword, "chain") == 0) {
	if (sscanf(s, "%2s %n", &atom.chain, &n) < 1) {
	  vtf_error("could not get chain in atom record", line);
	  return MOLFILE_ERROR;
	}
      }
    }
    case 'q': {
      /* q and charge */
      if (strlen(keyword) == 1 ||
	  strcmp(keyword, "charge") == 0) {
	if (sscanf(s, "%f %n", &atom.charge, &n) < 1) {
	  vtf_error("could not get charge in atom record", line);
	  return MOLFILE_ERROR;
	}
	d->optflags |= MOLFILE_CHARGE;
      } else {
	strcpy(msg, "unrecognized keyword in atom record: ");
	strncat(msg, keyword, 200);
	vtf_error(msg, line);
	return MOLFILE_ERROR;
      }
      s += n;
      break;
    }
    case 'a': {
      /* altloc, atomicnumber */
      if (strlen(keyword)== 1 || 
	  strcmp(keyword, "atomicnumber") == 0) {
	if (sscanf(s, "%d %n", &atom.atomicnumber, &n) < 1) {
	  vtf_error("could not get atomicnumber in atom record", line);
	  return MOLFILE_ERROR;
	}
	d->optflags |= MOLFILE_ATOMICNUMBER;
      } else if (strcmp(keyword, "altloc")) {
	if (sscanf(s, "%2s %n", &atom.altloc, &n) < 1) {
	  vtf_error("could not get altloc in atom record", line);
	  return MOLFILE_ERROR;
	}
	d->optflags |= MOLFILE_ALTLOC;
      } else { 
	strcpy(msg, "unrecognized keyword in atom record: ");
	strncat(msg, keyword, 200);
	vtf_error(msg, line);
	return MOLFILE_ERROR;
      }
      s += n;
      break;
    }
    case 'o': {
      /* occupancy */
      if (sscanf(s, "%f %n", &atom.occupancy, &n) < 1) {
	vtf_error("could not get occupancy in atom record", line);
	return MOLFILE_ERROR;
      }
      d->optflags |= MOLFILE_OCCUPANCY;
      s += n;
      break;
    }
    case 'b': {
      /* bfactor */
      if (sscanf(s, "%f %n", &atom.bfactor, &n) < 1) {
	vtf_error("could not get bfactor in atom record", line);
	return MOLFILE_ERROR;
      }
      d->optflags |= MOLFILE_BFACTOR;
      s += n;
      break;
    }
    case 'm': {
      /* mass */
      if (sscanf(s, "%f %n", &atom.mass, &n) < 1) {
	vtf_error("could not get mass in atom record", line);
	return MOLFILE_ERROR;
      }
      d->optflags |= MOLFILE_MASS;
      s += n;
      break;
    }
    default: { 
      /* unrecognized */
      strcpy(msg, "unrecognized keyword in atom record: ");
      strncat(msg, keyword, 200);
      vtf_error(msg, line);
      return MOLFILE_ERROR;
    }
    }
  }

  /* handle the aid specifier */

  /* if the specifier is "default", set the default_atom */
  if (aid_specifier[0] == 'd') {
    default_atom = atom;
  } else {
    /* otherwise parse the aid specifier */
    s = aid_specifier;
    while (1) {
      from = d->natoms;
      if (sscanf(s, "%u:%u%n", &from, &to, &n) == 2) {
	/* range given */
	if (from > to) { 
	  vtf_error("bad range specifier (from > to):", s); 
	  return MOLFILE_ERROR;
	}
	d->atoms = realloc(d->atoms, (to+1)*sizeof(molfile_atom_t));
	/* TODO: error handling */
	/* fill up with default atoms */
	for (aid = d->natoms; aid < to; aid++)
	  d->atoms[aid] = default_atom;
	/* create new atoms */
	if (to+1 > d->natoms) d->natoms = to+1;
	/* TODO: error handling */
	for (aid = from; aid <= to; aid++)
	  d->atoms[aid] = atom;
      } else if (sscanf(s, "%u%n", &to, &n) == 1) {
	/* single aid given */
	d->atoms = realloc(d->atoms, (to+1)*sizeof(molfile_atom_t));
	/* TODO: error handling */
	/* fill up with default atoms */
	for (aid = d->natoms; aid < to; aid++)
	  d->atoms[aid] = default_atom;
	/* create the new atom */
	if (to+1 > d->natoms) d->natoms = to+1;
	d->atoms[to] = atom;
      } else {
	vtf_error("bad atom specifier", s);
	return MOLFILE_ERROR;
      }

      /* advance s */
      s += n;

      /* if there is no more to parse, break */
      if (strlen(s) == 0) break;

      /* otherwise the next char should be a ',' */
      if (s[0] != ',') {
	vtf_error("bad atom specifier in line", line);
	return MOLFILE_ERROR;
      }
      /* skip the ',' */
      s++;
    };
  }

  return MOLFILE_SUCCESS;
}

/***************************************************
 * Parse BOND
 ***************************************************/
/* Parse bond data from line. 
   Return MOLFILE_SUCCESS, if data was sucessfully parsed, 
   MOLFILE_ERROR if an error occured. */
int vtf_parse_bond(char *line, vtf_data *d) {
  char *s;
  int n;
  int from, to, aid, bid;

  s = line;

  while (1) {
    if (sscanf(s, "%u::%u%n", &from, &to, &n) == 2) {
      /* chain specifier */
      if (from > to) {
	vtf_error("bad chain specifier (from > to):", s); 
	return MOLFILE_ERROR;
      }
      bid = d->nbonds;
      d->nbonds += to-from;
      d->from = realloc(d->from, d->nbonds*sizeof(int));
      d->to = realloc(d->to, d->nbonds*sizeof(int));
      /* TODO: error handling */
      for (aid = from; aid < to; aid++) {
	/*printf("creating bond from %d to %d\n", aid, aid+1);*/
	d->from[bid] = aid+1;
	d->to[bid] = aid+2;
	bid++;
      }
    } else if (sscanf(s, "%u:%u%n", &from, &to, &n) == 2) {
      /* single bond specifier */
      d->nbonds += 1;
      d->from = realloc(d->from, d->nbonds*sizeof(int));
      d->to = realloc(d->to, d->nbonds*sizeof(int));
      /* TODO: error handling */
      d->from[d->nbonds-1] = from+1;
      d->to[d->nbonds-1] = to+1;
    } else {
      vtf_error("bad bond specifier", s);
      return MOLFILE_ERROR;
    }

    s += n;
    
    /* if there is no more to parse, break */
    if (strlen(s) == 0) break;
    
    /* otherwise the next char should be a ',' */
    if (s[0] != ',') {
      vtf_error("bad bond specifier in line", line);
      return MOLFILE_ERROR;
    }
    /* skip the ',' */
    s++;
  }

  return MOLFILE_SUCCESS;
}


/***************************************************
 * Parse PBC
 ***************************************************/
/* Parse periodic boundary condition data from line. 
   Return MOLFILE_SUCCESS, if data was sucessfully parsed, 
   MOLFILE_ERROR if an error occured. */
int vtf_parse_pbc(char *line, vtf_data *d) {
  char *s;
  int n;

  if (sscanf(line, "%f %f %f %n", 
    &d->A, &d->B, &d->C, &n) < 3) {
    vtf_error("Couldn't parse unit cell dimensions", s);
    return MOLFILE_ERROR;
  }
  s = line+n;

  n = sscanf(s, "%f %f %f", &d->alpha, &d->beta, &d->gamma);
  if (n > 0 && n < 3) {
    vtf_error("Couldn't parse unit cell angles", line);
    return MOLFILE_ERROR;
  }
  return MOLFILE_SUCCESS;
} 

/* Parse timestep command from line. 
   Return MOLFILE_SUCCESS, if it was sucessfully parsed, 
   MOLFILE_ERROR if an error occured. */
int vtf_parse_timestep(char *line, vtf_data *d) {
  if (strlen(line) == 0) {
    d->next_indexed = 0;
  } else {
    switch (tolower(line[0])) {
    case 'o': { d->next_indexed = 0; break; }
    case 'i': { d->next_indexed = 1; break; }
    default: {
      vtf_error("bad timestep line", line);
      return MOLFILE_ERROR;
    }
    }
  }
  return MOLFILE_SUCCESS;
}

void vtf_parse_structure(vtf_data *d) {
  char *line;			/* next line in the file */
  char s[255];
  int n;
  
  /* initialize the default atom */
  strcpy(default_atom.name, "X");
  strcpy(default_atom.type, "X");
  strcpy(default_atom.resname, "X");
  default_atom.resid = 0;
  strcpy(default_atom.segid, "");
  strcpy(default_atom.chain, "");

  strcpy(default_atom.altloc, "");
  strcpy(default_atom.insertion, "");
  default_atom.occupancy = 1.0;
  default_atom.bfactor = 1.0;
  default_atom.mass = 1.0;
  default_atom.charge = 0.0;
  default_atom.radius = 1.0;

  /* initialize the data structure */
  d->optflags = MOLFILE_NOOPTIONS;
  d->natoms = MOLFILE_NUMATOMS_NONE;
  d->atoms = NULL;
  d->nbonds = 0;
  d->from = NULL;
  d->to = NULL;

  do {
    line = vtf_getline(d->file);
    if (line == NULL) break;
    switch (tolower(line[0])) {
      /* ATOM RECORD */
    case 'a': {
      /* Remove the "atom" keyword" */
      sscanf(line, "%255s %n", s, &n);
      line += n;
    }
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':	
    case 'd': { 
      /* parse atom */
      d->return_code = vtf_parse_atom(line, d);
      break; 
    }

      /* BOND RECORD */
    case 'b': {
      /* Remove the "bond" keyword" */
      sscanf(line, "%255s %n", s, &n);
      line += n;
      d->return_code = vtf_parse_bond(line, d);
      break;
    }

      /* PBC/UNITCELL RECORD */
    case 'u':
    case 'p': {
      /* Remove the "pbc" or "unitcell" keyword */
      sscanf(line, "%255s %n", s, &n);
      line += n;
      d->return_code = vtf_parse_pbc(line, d);
      break;
    }

      /* TIMESTEP RECORD*/
    case 'c': 
    case 't': {
      /* Remove the "timestep" or "coordinates" keyword */
      sscanf(line, "%255s %n", s, &n);
      line += n;
      
    }
    case 'i': 
    case 'o': { 
      d->return_code = vtf_parse_timestep(line, d);
      line = NULL; /* indicate the end of the structure block */
      break; 
    }

      /* UNKNOWN RECORD */
    default: {
      vtf_error("unknown line type", line);
      d->return_code = MOLFILE_ERROR;
      break;
    }
    }
  } while (line != NULL && 
	   d->return_code == MOLFILE_SUCCESS);

  /* test if structure data was parsed */
  if (d->atoms == NULL && 
      d->return_code == MOLFILE_SUCCESS) {
    d->return_code = MOLFILE_NOSTRUCTUREDATA;
  }

  /* test whether another error has occured */
  if (errno != 0) {
    perror("vtfplugin");
    d->return_code = MOLFILE_ERROR;
  }
}

/***************************************************
 * Open file and parse structure info
 ***************************************************/
/* Opens the file for reading. 
   To determine the number of atoms in the file, it is necessary to
   parse the structure information, anyway. Therefore, this function
   will do the parsing and save the information in the handle.
*/
void *vtf_open_file_read(const char *filepath, 
			 const char *filetype, 
			 int *natoms) {
  vtf_data *d;

  /* initialize the data structure */
  d = malloc(sizeof(vtf_data));

  /* Open the file */
  d->file = fopen(filepath, "r");
  if (d->file == NULL) {
    /* Could not open file */
    perror("vtfplugin");
    d->return_code = MOLFILE_ERROR;
    return NULL;
  }
  d->return_code = MOLFILE_SUCCESS;

  /* initialize timestep data */
  d->next_indexed = 0;
  d->coords = NULL;
  d->A = 0.0;
  d->B = 0.0;
  d->C = 0.0;
  d->alpha = 90.0;
  d->beta = 90.0;
  d->gamma = 90.0;

  if (strcmp(filetype, "vcf") == 0) {
    *natoms = MOLFILE_NUMATOMS_UNKNOWN;
  } else {
    vtf_parse_structure(d);
    
    if (d->return_code != MOLFILE_SUCCESS) {
      free(d);
      return NULL;
    }
    
    *natoms = d->natoms;
  } 

  return d;
}

int vtf_read_next_timestep(void *data, 
			   int natoms, 
			   molfile_timestep_t *ts) {
  vtf_data *d;
  int size;
  char *line;
  static char s[255];
  float x,y,z;
  char c;
  int aid;
  int n;

  d = (vtf_data*)data;

  aid = 0;

  if (feof(d->file)) return MOLFILE_EOF;

  if (d->coords == NULL) {
    /* initialize coords */
    d->coords = malloc(natoms*3*sizeof(float));
    /* TODO: error handling */
    for (n = 0; n < natoms*3; n++)
      d->coords[n] = 0.0;
  } 
  
  /* read in the data, until the next timestep or EOF is reached */
  do {
    line = vtf_getline(d->file);

    if (line == NULL) {
      if (errno != 0) {
	perror("vtfplugin");
	return MOLFILE_ERROR;
      }
      break;
    } 

    /* parse timestep data */
    if (!d->next_indexed && 
	sscanf(line, "%f %f %f", 
	       &d->coords[aid*3],
	       &d->coords[aid*3+1],
	       &d->coords[aid*3+2]) == 3) {
      aid++;
    } else if (d->next_indexed &&
	       sscanf(line, "%d%[ \t\f\v\n\r] %f %f %f", 
		      &aid, &c, &x, &y, &z) == 5) {
      d->coords[aid*3] = x;
      d->coords[aid*3+1] = y;
      d->coords[aid*3+2] = z;
    } else switch (tolower(line[0])) {
      /* PBC/UNITCELL RECORD */
    case 'u':
    case 'p': {
      /* Remove the "pbc" or "unitcell" keyword */
      sscanf(line, "%255s %n", s, &n);
      line += n;
      if (vtf_parse_pbc(line, d) != MOLFILE_SUCCESS) 
	return MOLFILE_ERROR;
      break;
    }
      
      /* TIMESTEP RECORD*/
    case 'c': 
    case 't': {
      /* Remove the "timestep" or "coordinates" keyword */
      sscanf(line, "%255s %n", s, &n);
      line += n;
    }
    case 'i': 
    case 'o': { 
      if (vtf_parse_timestep(line, d) != MOLFILE_SUCCESS)
	return MOLFILE_ERROR;
      line = NULL;
      break;
    }

    default: { 
      if (d->next_indexed)
	vtf_error("unknown line in indexed timestep block", line);
      else
	vtf_error("unknown line in ordered timestep block", line);
      return MOLFILE_ERROR;
    }
    }

    if (line == NULL) break;
  } while (1);

  if (ts != NULL) {
    /* copy the ts data */
    ts->A = d->A;
    ts->B = d->B;
    ts->C = d->C;
    ts->alpha = d->alpha;
    ts->beta = d->beta;
    ts->gamma = d->gamma;
    memcpy(ts->coords, d->coords, natoms*3*sizeof(float));
  }
  
  return MOLFILE_SUCCESS;
}

/***************************************************
 * Copy the info collected in vtf_open_file_read
 ***************************************************/
int vtf_read_structure(void *data, 
		       int *optflags, 
		       molfile_atom_t *atoms) {
  vtf_data *d;
  d = (vtf_data*)data;

  if (d->return_code != MOLFILE_SUCCESS) return d->return_code;

  if (d->natoms > 0) {
    /* copy the data parsed in vtf_open_file_read() */
    memcpy(atoms, d->atoms, d->natoms*sizeof(molfile_atom_t));
    /* free the data parsed in vtf_open_file_read() */
    free(d->atoms);
  }

  *optflags = d->optflags;

  return MOLFILE_SUCCESS;
}

int vtf_read_bonds(void *data, 
		   int *nbonds, 
		   int **from, 
		   int **to, 
		   float **bondorder) {
  vtf_data *d;
  d = (vtf_data*)data;

  *nbonds = d->nbonds;
  *from = d->from;
  *to = d->to;
  bondorder = NULL;

  return MOLFILE_SUCCESS;
}

void vtf_close_file_read(void *data) {
  vtf_data *d;
  d = (vtf_data*)data;

  /* free the bond information */
  if (d->from != NULL) free(d->from);
  if (d->to != NULL) free(d->to);

  /* close the file */
  fclose(d->file);
}

static molfile_plugin_t vsfplugin;
static molfile_plugin_t vtfplugin;
static molfile_plugin_t vcfplugin;

VMDPLUGIN_API int VMDPLUGIN_init() {
  memset(&vsfplugin, 0, sizeof(molfile_plugin_t));
  vsfplugin.abiversion = 9;
  vsfplugin.type = MOLFILE_PLUGIN_TYPE;
  vsfplugin.name = "vsf";
  vsfplugin.prettyname = "VMD structure format";
  vsfplugin.author = "Olaf Lenz";
  vsfplugin.majorv = 1;
  vsfplugin.minorv = 0;
  vsfplugin.is_reentrant = VMDPLUGIN_THREADUNSAFE;
  vsfplugin.filename_extension = "vsf";
  vsfplugin.open_file_read = vtf_open_file_read;
  vsfplugin.read_structure = vtf_read_structure;
  vsfplugin.read_bonds = vtf_read_bonds;
  /* plugin.read_next_timestep = vtf_read_next_timestep; */
  vsfplugin.close_file_read = vtf_close_file_read;

  memset(&vcfplugin, 0, sizeof(molfile_plugin_t));
  vcfplugin.abiversion = 9;
  vcfplugin.type = MOLFILE_PLUGIN_TYPE;
  vcfplugin.name = "vcf";
  vcfplugin.prettyname = "VMD coordinate format";
  vcfplugin.author = "Olaf Lenz";
  vcfplugin.majorv = 1;
  vcfplugin.minorv = 0;
  vcfplugin.is_reentrant = VMDPLUGIN_THREADUNSAFE;
  vcfplugin.filename_extension = "vcf";
  vcfplugin.open_file_read = vtf_open_file_read;
  vcfplugin.read_next_timestep = vtf_read_next_timestep;
  vcfplugin.close_file_read = vtf_close_file_read;

  memset(&vtfplugin, 0, sizeof(molfile_plugin_t));
  vtfplugin.abiversion = 9;
  vtfplugin.type = MOLFILE_PLUGIN_TYPE;
  vtfplugin.name = "vtf";
  vtfplugin.prettyname = "VMD trajectory format";
  vtfplugin.author = "Olaf Lenz";
  vtfplugin.majorv = 1;
  vtfplugin.minorv = 0;
  vtfplugin.is_reentrant = VMDPLUGIN_THREADUNSAFE;
  vtfplugin.filename_extension = "vtf";
  vtfplugin.open_file_read = vtf_open_file_read;
  vtfplugin.read_structure = vtf_read_structure;
  vtfplugin.read_bonds = vtf_read_bonds;
  vtfplugin.read_next_timestep = vtf_read_next_timestep;
  vtfplugin.close_file_read = vtf_close_file_read;

  return VMDPLUGIN_SUCCESS;
}

VMDPLUGIN_API int VMDPLUGIN_register(void *v, vmdplugin_register_cb cb) {
  (*cb)(v, (vmdplugin_t *)&vsfplugin);
  (*cb)(v, (vmdplugin_t *)&vcfplugin);
  (*cb)(v, (vmdplugin_t *)&vtfplugin);
  return VMDPLUGIN_SUCCESS;
}

VMDPLUGIN_API int VMDPLUGIN_fini() {
  return VMDPLUGIN_SUCCESS;
}

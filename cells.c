/** \file cells.c
 *
 *  This file contains everything related to the link cell
 *  algorithm. 
 *
 *  For more information on cells,
 *  see \ref cells.h "cells.h"
 *   */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cells.h"
#include "config.h"
#include "debug.h"
#include "grid.h"
#include "particle_data.h"
#include "interaction_data.h"
#include "integrate.h"
#include "communication.h"
#include "utils.h"
#include "verlet.h"
#include "ghosts.h"

/************************************************/
/** \name Defines */
/************************************************/
/*@{*/

/** half the number of cell neighbors in 3 Dimensions*/
#define CELLS_MAX_NEIGHBORS 14

/*@}*/

/************************************************/
/** \name Variables */
/************************************************/
/*@{*/

int cell_grid[3];
int ghost_cell_grid[3];
int n_cells;
int max_num_cells = CELLS_MAX_NUM_CELLS;
Cell *cells;

/** cell size. 
    Def: \verbatim cell_grid[i] = (int)(local_box_l[i]/max_range); \endverbatim */
double cell_size[3];
/** inverse cell size = \see cell_size ^ -1. */
double inv_cell_size[3];

double max_skin=0.0;

/*@}*/

/************************************************************/
/** \name Privat Functions */
/************************************************************/
/*@{*/

/** Calculate cell grid dimensions, cell sizes and number of cells.
 *  Calculates the cell grid, based on \ref local_box_l and \ref
 *  max_range. If the number of cells is larger than \ref
 *  max_num_cells, it increases max_range until the number of cells is
 *  smaller or equal \ref max_num_cells. It sets: \ref cell_grid, \ref
 *  ghost_cell_grid, \ref cell_size, \ref inv_cell_size, and \ref
 *  n_cells.
 */
void calc_cell_grid();

/** initialize a cell structure.
 *  Use with care and ONLY for initialization! 
 *  @param cell  Pointer to cell to initialize. */
void init_cell(Cell *cell);

/** initializes the interacting neighbor cell list of a cell
 *  (\ref #cells::neighbors).  the created list of interacting neighbor
 *  cells is used by the verlet algorithm (see verlet.c) to build the
 *  verlet lists.
 *
 *  @param i linear index of the cell.  */
void init_cell_neighbors(int i);
 
/*@}*/

/************************************************************/
void cells_pre_init()
{
  int i;
  CELL_TRACE(fprintf(stderr,"%d: cells_pre_init():\n",this_node));

  /* set cell grid variables to a (1,1,1) grid */
  for(i=0;i<3;i++) {
    cell_grid[i] = 1;
    ghost_cell_grid[i] = 3;
  }
  n_cells = 27;

  /* allocate space */
  cells = (Cell *)malloc(n_cells*sizeof(Cell));
  for(i=0; i<n_cells; i++) init_cell(&cells[i]);
}

/************************************************************/
void cells_re_init() 
{
  int i,j,ind;
  int old_n_cells, old_ghost_cell_grid[3];
  Cell         *old_cells;
  ParticleList *pl;
  Particle     *part;

#ifdef ADDITIONAL_CHECKS
  int part_cnt_old, part_cnt_new;
#endif

  /* first move particles to their nodes. Necessary if
     box length has changed. */
  invalidate_ghosts();
  exchange_and_sort_part();

  CELL_TRACE(fprintf(stderr,"%d: cells_re_init \n",this_node));

  /* 1: store old cell grid */
  old_cells = cells;
  old_n_cells = n_cells;
  for(i=0;i<3;i++) old_ghost_cell_grid[i] = ghost_cell_grid[i];
 
  /* 2: setup new cell grid */
  /* 2a: set up dimensions of the cell grid */
  calc_cell_grid();  
  /* 2b: there should be a reasonable number of cells only!
     But we will deal with that later... */
  /* 2c: allocate new cell structure */
  cells  = (Cell *)malloc(n_cells*sizeof(Cell));
  /* 2d: allocate particle arrays */
  for(i=0; i<n_cells; i++) init_cell(&cells[i]);
  /* 2e: init cell neighbors */
  for(i=0; i<n_cells; i++) init_cell_neighbors(i);
 
  /* 3: Transfer Particle data from old to new cell grid */
  for(i=0;i<old_n_cells;i++) {
    pl = &(old_cells[i].pList);
    if(is_inner_cell(i,old_ghost_cell_grid)) {
      for(j=0; j<pl->n; j++) {
	part = &(pl->part[j]);
	ind  = pos_to_cell_grid_ind(part->r.p);
	append_unindexed_particle(&(cells[ind].pList),part);
      }
    }
    if(pl->max>0) free(pl->part);
    if(old_cells[i].n_neighbors>0) {
      for(j=0; j<old_cells[i].n_neighbors; j++) free(old_cells[i].nList[j].vList.pair);
      free(old_cells[i].nList);
    }
  }

  for(i=0;i<n_cells;i++) {
    if(is_inner_cell(i,ghost_cell_grid))
      update_local_particles(&(cells[i].pList));
  }

#ifdef ADDITIONAL_CHECKS
  /* check particle transfer */
  part_cnt_old=0;
  for(i=0;i<old_n_cells;i++) 
    if(is_inner_cell(i,old_ghost_cell_grid)) 
      part_cnt_old += old_cells[i].pList.n;
  part_cnt_new=0;
  for(i=0;i<n_cells;i++) 
    if(is_inner_cell(i,ghost_cell_grid)) 
      part_cnt_new += cells[i].pList.n;
  if(part_cnt_old != part_cnt_new) 
    CELL_TRACE(fprintf(stderr,"%d: cells_re_init: lost particles: old grid had %d new grid has %d particles.\n",this_node,part_cnt_old, part_cnt_new));
#endif

  free(old_cells);
  /* cell structure initialized. */
  rebuild_verletlist = 1;
}



/*************************************************/
void cells_changed_topology()
{
  int i;
  if(max_range <= 0) {
    /* not yet fully initialized */
    max_range = min_local_box_l/2.0; }
  
  for(i=0;i<3;i++) {
    cell_size[i] =  local_box_l[i];
    inv_cell_size[i] = 1.0 / cell_size[i];
  }

  cells_re_init();
}

/*************************************************/
int cells_get_n_particles()
{
  int cnt = 0, m, n, o;
  INNER_CELLS_LOOP(m, n, o)
    cnt += CELL_PTR(m, n, o)->pList.n;
  return cnt;
}

/*************************************************/
Particle *cells_alloc_particle(int id, double pos[3])
{
  int rl;
  int ind = pos_to_cell_grid_ind(pos);
  ParticleList *pl = &cells[ind].pList;
  Particle *pt;

  pl->n++;
  rl = realloc_particles(pl, pl->n);
  pt = &pl->part[pl->n - 1];
  init_particle(pt);

  pt->r.identity = id;
  memcpy(pt->r.p, pos, 3*sizeof(double));
  if (rl)
    update_local_particles(&cells[ind].pList);
  else
    local_particles[pt->r.identity] = pt;

  return pt;
}

/*************************************************/
int pos_to_cell_grid_ind(double pos[3])
{
  int i,cpos[3];
  
  for(i=0;i<3;i++) {
    cpos[i] = (int)((pos[i]-my_left[i])*inv_cell_size[i])+1;

#ifdef PARTIAL_PERIODIC
    if(periodic[i] == 0) {
      if (cpos[i] < 1)                 cpos[i] = 1;
      else if (cpos[i] > cell_grid[i]) cpos[i] = cell_grid[i];
    }
#endif

#ifdef ADDITIONAL_CHECKS
    if(cpos[i] < 1 || cpos[i] >  cell_grid[i]) {
      fprintf(stderr,"%d: illegal cell position cpos[%d]=%d, ghost_grid[%d]=%d for pos[%d]=%f\n",this_node,i,cpos[i],i,ghost_cell_grid[i],i,pos[i]);
      errexit();
    }
#endif

  }
  return get_linear_index(cpos[0],cpos[1],cpos[2], ghost_cell_grid);  
}

/*************************************************/
int pos_to_capped_cell_grid_ind(double pos[3])
{
  int i,cpos[3];
  
  for(i=0;i<3;i++) {
    cpos[i] = (int)((pos[i]-my_left[i])*inv_cell_size[i])+1;

    if (cpos[i] < 1)
      cpos[i] = 1;
    else if (cpos[i] > cell_grid[i])
      cpos[i] = cell_grid[i];
  }
  return get_linear_index(cpos[0],cpos[1],cpos[2], ghost_cell_grid);  
}


/*************************************************/

int max_num_cells_callback(Tcl_Interp *interp, void *_data)
{
  int data = *(int *)_data;
  if (data < 27) {
    Tcl_AppendResult(interp, "WARNING: max_num_cells has to be at least 27. Set max_num_cells = 27!", (char *) NULL);
    data = 27;
  }
  max_num_cells = data;
  mpi_bcast_parameter(FIELD_MAXNUMCELLS);
  mpi_bcast_event(PARAMETER_CHANGED);
  mpi_bcast_event(TOPOLOGY_CHANGED);
  return (TCL_OK);
}

/*************************************************/
int  is_inner_cell(int i, int gcg[3])
{
  int pos[3];
  get_grid_pos(i,&pos[0],&pos[1],&pos[2],gcg);
  return (pos[0]>0 && pos[0] < gcg[0]-1 &&
	  pos[1]>0 && pos[1] < gcg[1]-1 &&
	  pos[2]>0 && pos[2] < gcg[2]-1);
}


/************************************************************/
/*******************  privat functions  *********************/
/************************************************************/

void calc_cell_grid()
{
  int i;
  double cell_range;

  /* normal case */
  n_cells=1;
  for(i=0;i<3;i++) {
    ghost_cell_grid[i] = (int)(local_box_l[i]/max_range) + 2;
    n_cells *= ghost_cell_grid[i];
  }

  /* catch case, n_cells > max_num_cells */
  if(n_cells > max_num_cells) {
    int count;
    double step;
    double min_box_l;
    double max_box_l;

    min_box_l = dmin(dmin(local_box_l[0],local_box_l[1]),local_box_l[2]);
    max_box_l = dmax(dmax(local_box_l[0],local_box_l[1]),local_box_l[2]);
    step = ((max_box_l/2.0)-max_range)/100; /* Maximal 100 trials! */
    if(step<0.0) {
      fprintf(stderr,"%d: calc_cell_grid: Error: negative step! Ask your local Guru\n",this_node);
      errexit();
    }
    cell_range = max_range;

    count = 100;
    while(n_cells > max_num_cells && --count > 0) {
      cell_range += step;
      n_cells=1;
      for(i=0;i<3;i++) {
	ghost_cell_grid[i] = (int)(local_box_l[i]/cell_range) + 2;
	/* make sure that at least one inner cell exists in all directions.
	   Helps with highly anisotropic systems. */
	if (ghost_cell_grid[i] <= 2)
	  ghost_cell_grid[i] = 3;
	n_cells *= ghost_cell_grid[i];
      }
    }
    if (count == 0) {
      fprintf(stderr, "%d: calc_cell_grid: Error: no suitable cell grid found (max_num_cells was %d)\n",
	      this_node,max_num_cells);
      errexit();
    }
    /* Store information about possible larger skin. */
  }
  cell_range = dmin(dmin(cell_size[0],cell_size[1]),cell_size[2]);
  max_skin = cell_range - max_cut;

  /* now set all dependent variables */
  for(i=0;i<3;i++) {
    cell_grid[i] = ghost_cell_grid[i]-2;	
    cell_size[i]     = local_box_l[i]/(double)cell_grid[i];
    inv_cell_size[i] = 1.0 / cell_size[i];
  }
}

/************************************************************/
void init_cell(Cell *cell)
{
  cell->n_neighbors = 0;
  cell->nList       = NULL;
  init_particleList(&(cell->pList));
}

/************************************************************/
void init_cell_neighbors(int i)
{
  int j,m,n,o,cnt=0;
  int p1[3],p2[3];

  if(is_inner_cell(i,ghost_cell_grid)) { 
    cells[i].nList = (IA_Neighbor *) realloc(cells[i].nList,CELLS_MAX_NEIGHBORS*sizeof(IA_Neighbor));    
    get_grid_pos(i,&p1[0],&p1[1],&p1[2], ghost_cell_grid);
    /* loop through all neighbors */
    for(m=-1;m<2;m++)
      for(n=-1;n<2;n++)
	for(o=-1;o<2;o++) {
	  p2[0] = p1[0]+o;   p2[1] = p1[1]+n;   p2[2] = p1[2]+m;
	  j = get_linear_index(p2[0],p2[1],p2[2], ghost_cell_grid);
	  /* take the upper half of all neighbors 
	     and add them to the neighbor list */
	  if(j >= i) {
	    CELL_TRACE(fprintf(stderr,"%d: cell %d neighbor %d\n",this_node,i,j));
	    cells[i].nList[cnt].cell_ind = j;
	    cells[i].nList[cnt].pList = &(cells[j].pList);
	    init_pairList(&(cells[i].nList[cnt].vList));
	    cnt++;
	  }
	}
    cells[i].n_neighbors = cnt;
  }
  else { 
    cells[i].n_neighbors = 0;
  }   
}

/*************************************************/
void print_particle_positions()
{
  int i,m,n,o,np,cnt=0;
  ParticleList *pl;
  Particle *part;

  INNER_CELLS_LOOP(m, n, o) {
    pl   = &(CELL_PTR(m, n, o)->pList);
    part = CELL_PTR(m, n, o)->pList.part;
    np   = CELL_PTR(m, n, o)->pList.n;
    for(i=0 ; i<pl->n; i++) {
      fprintf(stderr,"%d: cell(%d,%d,%d) Part id=%d pos=(%f,%f,%f)\n",
	      this_node, m, n, o, part[i].r.identity,
	      part[i].r.p[0], part[i].r.p[1], part[i].r.p[2]);
      cnt++;
    }
  }
  fprintf(stderr,"%d: Found %d Particles\n",this_node,cnt);
}

/*************************************************/
void print_ghost_positions()
{
  int i,m,n,o,np,cnt=0;
  ParticleList *pl;
  Particle *part;

  CELLS_LOOP(m, n, o) {
    if(IS_GHOST_CELL(m,n,o)) {
      pl   = &(CELL_PTR(m, n, o)->pList);
      part = CELL_PTR(m, n, o)->pList.part;
      np   = CELL_PTR(m, n, o)->pList.n;
      for(i=0 ; i<pl->n; i++) {
	fprintf(stderr,"%d: cell(%d,%d,%d) ghost id=%d pos=(%f,%f,%f)\n",
		this_node, m, n, o, part[i].r.identity,
		part[i].r.p[0], part[i].r.p[1], part[i].r.p[2]);
	cnt++;
      }
    }
  }
  fprintf(stderr,"%d: Found %d Ghosts\n",this_node,cnt);
}

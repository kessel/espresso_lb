/** \file ghosts.c   Ghost particles and particle exchange.
 *
 *  For more information on ghosts,
 *  see \ref ghosts.h "ghosts.h" 
*/
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ghosts.h"
#include "debug.h"
#include "global.h"
#include "cells.h"
#include "communication.h"
#include "grid.h"
#include "utils.h"

/************************************************
 * defines
 ************************************************/

/**  granularity of the communication buffers. */
#define PART_INCREMENT 20

/* MPI tags for the ghost communications: */
/** Tag for communication in send_particles(). */
#define REQ_SEND_PART   100
/** Tag for communication in send_ghosts(). */
#define REQ_SEND_GHOSTS 101
/** Tag for communication in send_posforce(). */
#define REQ_SEND_POS    102

/** \name Variables for particle exchange */
/************************************************************/
/*@{*/

/** Particle send buffer. */
ParticleList p_send_buf;
/** Particle recieve buffer. */
ParticleList p_recv_buf;

/** Bond send buffer. */
IntList b_send_buf;
/** Bond recieve buffer. */
IntList b_recv_buf;
/*@}*/

/** \name Variables for ghost particle exchange */
/************************************************************/
/*@{*/

/** Ghost send buffer. */
RedParticleList g_send_buf;
/** Ghost receive buffer. */
RedParticleList g_recv_buf;

/** List of cell indices to send. */
IntList send_cells[6];
/** List of cell indices to receive. */
IntList recv_cells[6];

/** List with number of ghosts in send cells 
    + one (last) entry for total number. */
IntList n_send_ghosts[6];
/** List with number of ghosts in recieve cells 
    + one (last) entry for total number. */
IntList n_recv_ghosts[6];

/** Total number of ghosts to send in one direction . */
int ghost_send_size[6];
/** Total number of ghosts to recv from one direction. */
int ghost_recv_size[6];

/*@}*/

/** \name Variables for ghost force/position exchange */
/************************************************************/
/*@{*/
/** Buffer for forces/coordinates to send. */
DoubleList send_buf;
/** Buffer for forces/coordinates to recieve. */
DoubleList recv_buf;
/*@}*/

/** \name Privat Functions */
/************************************************************/
/*@{*/

/** Creates a linear index list of a sub grid.
 *
 *  The sub grid is defined by its lower and upper corner:
 *  from (lc[0],lc[1],lc[2]) to (hc[0],hc[1],hc[2])
 *  The grid dimension is given with (gs[0],gs[1],gs[2])
 *  The linear index list of length <returnvalue> is stored 
 *  in list starting from position start. max should be the 
 *  total length of list to ensure that the indices fit into list.
 *
 * @return         size of the subgrid.
 * @param *list    array to store the indices of the sub grid.
 * @param start    start index for sub grid indices.
 * @param max      size of the array list.
 * @param lc[3]    lower corner of sub grid.
 * @param hc[3]    upper corner of sub grid.
 * @param gs[3]    grid dimension .
 */  
int sub_grid_indices(int* list, int start, int max,
		     int lc[3], int hc[3], int gs[3]);

/** moves particle (ind) to the send buffers. 
 * 
 *  subroutine of \ref exchange_part .  
 *
 *  Moves one particle (struct: Particle, local index: ind) to the
 *  send buffer (p_send_buf, b_send_buf) and removes it from the local
 *  particle field. The empty space in the local particle field is
 *  then filled with the last particle (See illustration).
 *
 *  \image html move_to_p_buf.gif "Move particle to send buffer" 
 *  \image latex move_to_p_buf.eps "Move particle to send buffer" \width=10cm
 *  
 *  Variable changes:
 *  <ul> 
 *    <li> n_p_send_buf += 1
 *    <li> n_b_send_buf += number of bond partners
 *    <li> remove particle from local_index field (set to -1)
 *    <li> actualize local_index entry for last particle
 *    <li> n_particles -= 1
 *  </ul>
 *  Reallocation of p_send_buf and b_send_buf if necessary.  
 *
 * \warning \b Supports only two particle bonds at the moment
 *
 * @return    local index to continue the particle loop.     
 * @param ind local index of the particle. */
int move_to_p_buf(ParticleList *pl, int ind);

/** send particles in direction s_dir.
 *
 *  subroutine of \refexchange_part. 
 *
 *  Check if communication goes to a different node.
 *  <ol>
 *    <li> Communication to a different node:
           Two step communication (first all even PEs send than all odd)
	   Send buffer sizes.
	   Reallocate recieve buffers if necessary.
	   Send particle and bond buffers.
      <li> Communication inside one node
           Exchange adress and sizes of send and recieve buffers.
 *  </ol>
 *
 * \warning \b Supports only two particle bonds at the moment
 *
 * @param s_dir    send direction. 
*/
void send_particles(int s_dir);

/** appends recieved particles of direction dir to local particle array.
 *
 *  subroutine of \ref exchange_part. 
 *
 *  Folds the coordinate in the send/recv direction.
 *  Reallocate particle buffer (particles) if necessary. Copy
 *  particles and their bonds from recieve buffers to local particle
 *  buffer.
 *
*/
void append_particles(int dir);

/** Send ghost particles in direction s_dir.
 *
 *  subroutine of \ref exchange_ghost. 
 *
 *  Does an unbuffered communication from all nodes to their neighbor
 *  node in direction s_dir.
 *  <ol>
 *      <li> send number of ghost in each send cell for this direction:
 *           \ref n_send_ghosts to \ref n_recv_ghosts.
 *           The total number of ghosts to send/receive is in 
 *           \ref n_send_ghosts[\ref max_send_cells].
 *      <li> send ghost particle information:
 *           \ref g_send_buf to \ref g_recv_buf
 *  </ol>
 *
 *  If communication goes to the same node, just the pointers of the
 *  array pairs (\ref g_send_buf, \ref g_recv_buf) and the variable
 *  pairs (\ref max_g_send_buf, \ref max_g_recv_buf) and (\ref
 *  n_g_send_buf, \ref n_g_recv_buf) are exchanged.
 *
 * @param s_dir send direction.  
*/
void send_ghosts(int s_dir);


/** send positions/forces in direction s_dir.
 *
 *  Does an unbuffered communication from all nodes to their neighbor
 *  node in direction s_dir.
 *
 *  send positions / forces: \ref send_buf to \ref recv_buf.
 *  If communication goes to the same node just the pointers 
 *  of \ref send_buf and \ref recv_buf are exchanged.
 *
 * @param s_dir       direction to send to/recv from.
 * @param send_size   number of positions/forces to send. 
 * @param recv_size   number of positions/forces to receive.
 */
void send_posforce(int s_dir,int send_size, int recv_size);

/*@}*/
/************************************************************/

void ghost_init()
{
  int i;
  /* ghost cell grid, cell grid */
  int gcg[3],cg[3];
  /* cell list sizes for the 3 dimensions. */
  int anz[3];
  /* send/recv frames start indizes, end indizes */
  int lc[3],hc[3],done[3]={0,0,0};

  GHOST_TRACE(fprintf(stderr,"%d: ghost_init:\n",this_node));
  GHOST_TRACE(fprintf(stderr,"%d: box_l=(%f,%f,%f):\n",this_node,box_l[0],box_l[1],box_l[2]));
  GHOST_TRACE(fprintf(stderr,"%d: boundary=(%d,%d,%d,%d,%d,%d):\n",this_node,
		      boundary[0],boundary[1],boundary[2],boundary[3],boundary[4],boundary[5]));

  /* Init ghost exchange */
  /* preparation of help variables */
  for(i=0;i<3;i++) {
    gcg[i] = ghost_cell_grid[i];
    cg[i]  = cell_grid[i];
  }
  anz[0] = cg[1] *cg[2];
  anz[1] = cg[2] *gcg[0];
  anz[2] = gcg[0]*gcg[1];

  /* create send/recv cell index lists for ghost exchange*/
  for(i=0;i<6;i++) {
    init_intlist(&(send_cells[i]));
    realloc_intlist(&(send_cells[i])   ,anz[i/2]);
    send_cells[i].n = anz[i/2];

    init_intlist(&(recv_cells[i]));
    realloc_intlist(&(recv_cells[i])   ,anz[i/2]);
    recv_cells[i].n = anz[i/2];

    init_intlist(&(n_send_ghosts[i]));
    realloc_intlist(&(n_send_ghosts[i]),anz[i/2]+1);
    n_send_ghosts[i].n = anz[i/2]+1;
  
    init_intlist(&(n_recv_ghosts[i]));
    realloc_intlist(&(n_recv_ghosts[i]),anz[i/2]+1);
    n_recv_ghosts[i].n = anz[i/2]+1;
  }

  /* direction loop (sorry, it looks nasty, and it is!!!). */
  for(i=0;i<3;i++) {
    lc[(i+1)%3] = 1-done[(i+1)%3]; hc[(i+1)%3] = cg[(i+1)%3]+done[(i+1)%3];
    lc[(i+2)%3] = 1-done[(i+2)%3]; hc[(i+2)%3] = cg[(i+2)%3]+done[(i+2)%3];
    /* send to :   left, down, for */
    lc[(i+0)%3] = 1;               hc[(i+0)%3] = 1;
    send_cells[(2*i)].n = sub_grid_indices(send_cells[(2*i)].e, 0, 
					     anz[i], lc, hc, gcg);
     /* recv from : right, up, back */
    lc[(i+0)%3] = 0;               hc[(i+0)%3] = 0;
    recv_cells[(2*i)].n = sub_grid_indices(recv_cells[(2*i)].e, 0, 
					     anz[i], lc, hc, gcg);
    /* send to :   right, up, back */
    lc[(i+0)%3] = cg[(i+0)%3];     hc[(i+0)%3] = cg[(i+0)%3];
    send_cells[(2*i)+1].n = sub_grid_indices(send_cells[(2*i)+1].e, 0, 
					     anz[i], lc, hc, gcg);
    /* recv from : left, down, for */
    lc[(i+0)%3] = cg[(i+0)%3]+1;   hc[(i+0)%3] = cg[(i+0)%3]+1;
    recv_cells[(2*i)+1].n = sub_grid_indices(recv_cells[(2*i)+1].e, 0, 
					   anz[i], lc, hc, gcg);
    done[i] = 1;
  }
  
  /* init some more lists */
  init_particleList(&p_send_buf);
  init_particleList(&p_recv_buf);
  init_intlist(&b_send_buf);
  init_intlist(&b_recv_buf);  
  init_redParticleList(&g_send_buf);
  init_redParticleList(&g_recv_buf);
  init_doublelist(&send_buf);
  init_doublelist(&recv_buf);
}

void exchange_and_sort_part()
{
  ParticleList *pl;
  Particle *part = NULL;
  Cell *cell = NULL;
  int d, i = 0, m, n, o, np, dir, lr, ind, c;
  int cell_part_cnt=0, ghost_part_cnt=0, local_part_cnt=0;

  GHOST_TRACE(fprintf(stderr,"%d: exchange_part:\n",this_node));

  for(d=0; d<3; d++) { /* direction loop */  
    if(node_grid[d] > 1) { /* catch single node case for direction dir! */
      for(lr=0; lr<2; lr++) {
	dir = 2*d + lr;
	p_send_buf.n = p_recv_buf.n = 0;
	b_send_buf.n = b_recv_buf.n = 0;
	INNER_CELLS_LOOP(m, n, o) {
	  c    = CELL_IND(m, n, o);
	  cell = &cells[c];
	  pl   = &(cell->pList);
	  part = pl->part;
	  np   = pl->n;
	  for(i=0 ; i< np; i++) {
	    /*fprintf(stderr,"%d: exchange_part %d: Part id=%d co=%f\n",
	      this_node,dir,part[i].r.identity,part[i].r.p[d]);*/
	    if((lr == 1 && part[i].r.p[d] >=  my_right[d]) ||
	       (lr == 0 && part[i].r.p[d] <  my_left[d])) {
	      GHOST_TRACE(fprintf(stderr,"%d: exchange_part: Send Part id=%d to node %d\n",
				  this_node,part[i].r.identity,node_neighbors[dir]));
	      i = move_to_p_buf(pl,i);
	    }
	    else if (dir == 5) {
	      /* sort particles not moved into their real cells */
	      ind = pos_to_cell_grid_ind(part[i].r.p);
	      if(ind != c) {
		move_indexed_particle(&(cells[ind].pList), pl, i);
		i--;
	      }
	    }
	  }
	}
	send_particles(dir);
	append_particles(d);
      }
    }
    else {
      /* in case of single node grid, fold coordinates */
      INNER_CELLS_LOOP(m, n, o) {
	c    = CELL_IND(m, n, o);
	cell = &cells[c];
	pl   = &(cell->pList);
	part = pl->part;
	np   = pl->n;
	for(i=0 ; i<np; i++) {
	  fold_coordinate(part[i].r.p, part[i].i, d);

	  if (d==3) {
	    /* sort particles not moved into their real cells */
	    ind = pos_to_cell_grid_ind(part[i].r.p);
	    if(ind != c) {
	      move_indexed_particle(&(cells[ind].pList), pl, i);
	      i--;
	    }
	  }
	}
      }
    }
  }

  INNER_CELLS_LOOP(m, n, o)
    update_local_particles(&(CELL_PTR(m, n, o)->pList));


#ifdef ADDITIONAL_CHECKS
  for(c=0; c<n_cells; c++) {
    if(is_inner_cell(c,ghost_cell_grid)) {
      cell_part_cnt += cells[c].pList.n;
      cell = &cells[c];
      pl   = &(cell->pList);
      part = pl->part;
      np   = pl->n;
      for(n=0; n<cells[c].pList.n ; n++) {
	if(part[n].r.identity < 0 || part[n].r.identity > max_seen_particle) {
	  fprintf(stderr,"%d: sort_part_in_cells: ERROR: Cell %d Part %d has corrupted id=%d\n",
		  this_node,c,n,cells[c].pList.part[n].r.identity);
	  errexit();
	}
	for(dir=0;dir<3;dir++) {
	  if(periodic[dir] && (part[n].r.p[dir] < 0 || part[n].r.p[dir] > box_l[dir])) {
	    fprintf(stderr,"%d: exchange_part: ERROR: illegal pos[%d]=%f of part %d id=%d in cell %d\n",
		    this_node,dir,part[n].r.p[dir],n,part[n].r.identity,c);
	    errexit();
	  }
	}
	if(local_particles[part[n].r.identity] != &part[n]) {
	    fprintf(stderr,"%d: exchange_part: ERROR: address mismatch for part id %d: %p %p in cell %d\n",
		    this_node,part[n].r.identity,local_particles[part[n].r.identity],
		    &part[n],c);
	    errexit();

	}
      }
    }
    else {
      if(cells[c].pList.n>0) {
	ghost_part_cnt += cells[c].pList.n;
	fprintf(stderr,"%d: sort_part_in_cells: WARNING: ghost_cell %d contains %d particles!\n",
		this_node,c,cells[c].pList.n);
      }
    }
  }
  CELL_TRACE(fprintf(stderr,"%d: sort_part_in_cells: %d particles in cells.\n",
		     this_node,cell_part_cnt));
  for(n=0; n< max_seen_particle+1; n++) {
    if(local_particles[n] != NULL) {
      local_part_cnt ++;
      if(local_particles[n]->r.identity != n) {
	fprintf(stderr,"%d: sort_part_in_cells: ERROR: local_particles part %d has corrupted id %d\n",
		this_node,n,local_particles[n]->r.identity);
	errexit();
      }
    }
  }
  CELL_TRACE(fprintf(stderr,"%d: sort_part_in_cells: %d particles in local_particles.\n",
		     this_node,local_part_cnt));
  if(local_part_cnt != cell_part_cnt) {
    fprintf(stderr,"%d: sort_part_in_cells: ERROR: %d parts in cells but %d parts in local_particles\n",
	    this_node,local_part_cnt,cell_part_cnt);
    if(ghost_part_cnt==0) errexit();
  }
  if(ghost_part_cnt>0) {
    fprintf(stderr,"%d: sort_part_in_cells: ERROR: Found %d illegal ghost particles!\n",
	    this_node,ghost_part_cnt);
    errexit();
  }
#endif

  GHOST_TRACE(print_particle_positions());
}

void invalidate_ghosts()
{
  Particle *part;
  int m, n, o, i, np;
  /* remove ghosts, but keep Real Particles */
  CELLS_LOOP(m, n, o) {
    /* ghost cell selection */
    if (m == 0 || m == ghost_cell_grid[0] - 1 ||
	n == 0 || n == ghost_cell_grid[1] - 1 ||
	o == 0 || o == ghost_cell_grid[2] - 1) {
       part = CELL_PTR(m, n, o)->pList.part;
       np   = CELL_PTR(m, n, o)->pList.n;
       for(i=0 ; i<np; i++) {
	 /* Particle is stored as ghost in the local_particles array,
	    if the pointer stored there belongs to a ghost celll
	    particle array. */
	 if( &(part[i]) == local_particles[part[i].r.identity] ) 
	   local_particles[part[i].r.identity] = NULL;
       }
       np   = CELL_PTR(m, n, o)->pList.n = 0;
    }
  }
}

void exchange_ghost()
{
  int s_dir=0, r_dir=0;
  int c, c_max, n, cnt;
  ParticleList *pl;

  GHOST_TRACE(fprintf(stderr,"%d: exchange_ghost:\n",this_node));

  g_send_buf.max = 0;
  g_recv_buf.max = 0;

  for(s_dir=0; s_dir<6; s_dir++) {                      /* direction loop */
    if(s_dir%2 == 0) r_dir = s_dir+1;
    else             r_dir = s_dir-1;

    /* number of ghosts to send */
    c_max                         = send_cells[s_dir].n;
    n_send_ghosts[s_dir].e[c_max] = 0;
    cnt                           = 0;
    for(c=0; c<c_max; c++) {
      pl = &(cells[send_cells[s_dir].e[c]].pList);
      n_send_ghosts[s_dir].e[c]      = pl->n;
      n_send_ghosts[s_dir].e[c_max] += pl->n;
    }
    if(n_send_ghosts[s_dir].e[c_max] > g_send_buf.max) 
      realloc_redParticles(&g_send_buf, n_send_ghosts[s_dir].e[c_max]);

    /* copy ghosts to send buffer */
    for(c=0; c<c_max; c++) {
      pl = &(cells[send_cells[s_dir].e[c]].pList);
      for(n=0; n<pl->n; n++) {
	memcpy(&(g_send_buf.part[cnt]), &(pl->part[n].r), sizeof(ReducedParticle));
	/* fold ghost coordinates if they cross the boundary */
	switch(boundary[s_dir]) {
	case 0: break;
	case 1:
	  g_send_buf.part[cnt].p[s_dir/2] += box_l[s_dir/2];
	  break;
	case -1:
	  g_send_buf.part[cnt].p[s_dir/2] -= box_l[s_dir/2];
	  break;
	}
	cnt++;
      }
    }

    /* send ghosts */
    send_ghosts(s_dir);

    /* copy recieved ghosts to cell structure */

    c_max = recv_cells[r_dir].n;
    cnt=0;
    for(c=0; c<c_max; c++) {
      pl    = &(cells[recv_cells[r_dir].e[c]].pList);
      realloc_particles(pl, n_recv_ghosts[r_dir].e[c]);
      pl->n = n_recv_ghosts[r_dir].e[c];
      for(n=0; n < pl->n; n++) {
	memcpy(&(pl->part[n].r), &(g_recv_buf.part[cnt]), sizeof(ReducedParticle));
	/* Real Paricle Priority! 
	   If Ghost Particle is already present on that node, 
	   than leave local index unchanged! */
	if( local_particles[pl->part[n].r.identity] == NULL )
	  local_particles[pl->part[n].r.identity] = &(pl->part[n]);
	cnt++;
      }
    }
  }

  /* realloc pos/force buffers */
  if(g_send_buf.max > g_recv_buf.max) 
    send_buf.n = recv_buf.n = 3*g_send_buf.max;
  else 
    send_buf.n = recv_buf.n = 3*g_recv_buf.max;
  
  realloc_doublelist(&send_buf, send_buf.n);
  realloc_doublelist(&recv_buf, recv_buf.n);

  GHOST_TRACE(fprintf(stderr,"%d: ghost_send_size=(%d,%d,%d,%d,%d,%d), send_buf.max=%d\n",
		      this_node,ghost_send_size[0], ghost_send_size[1], 
		      ghost_send_size[2],ghost_send_size[3], ghost_send_size[4], 
		      ghost_send_size[5],send_buf.max));
  GHOST_TRACE(fprintf(stderr,"%d: ghost_recv_size=(%d,%d,%d,%d,%d,%d), recv_buf.max=%d\n",
		      this_node,ghost_recv_size[0], ghost_recv_size[1], 
		      ghost_recv_size[2],ghost_recv_size[3], ghost_recv_size[4], 
		      ghost_recv_size[5],recv_buf.max));

  /* GHOST_TRACE(print_ghost_positions());*/
}

void update_ghost_pos()
{
  int s_dir, r_dir;
  int c, n, g, i;
  int mod_ind;
  double modifier;
  ParticleList *pl;
 
  /*  GHOST_TRACE(fprintf(stderr,"%d: update_ghost_pos:\n",this_node)); */

  for(s_dir=0; s_dir<6; s_dir++) {          /* direction loop forward */
    if(s_dir%2 == 0) r_dir = s_dir+1;
    else             r_dir = s_dir-1;
    mod_ind = s_dir/2;
    switch(boundary[s_dir]) {
    case 0:  modifier =  0;            break;
    case 1:  modifier =  box_l[s_dir/2]; break;
    case -1: modifier = -box_l[s_dir/2]; break;
    default: fprintf(stderr, "boundary conditions corrupt, exiting\n");
      errexit(); /* never reached */; modifier = 0;
    }
    g=0;

    /* loop send cells -  copy positions to buffer*/
    for(c=0; c<send_cells[s_dir].n; c++) {
      pl = &(cells[send_cells[s_dir].e[c]].pList);
      for(n=0; n < pl->n; n++) {
	for(i=0;i<3;i++) send_buf.e[g+i] = pl->part[n].r.p[i];
	/* fold positions if they cross the boundary */
	send_buf.e[g+mod_ind] += modifier;
	g += 3;
      }
    }

    /* send buffer */
    send_posforce(s_dir, ghost_send_size[s_dir], ghost_recv_size[r_dir]);

    /* loop recv cells - copy positions from buffer */
    g=0;
    for(c=0; c<recv_cells[r_dir].n; c++) {
      pl = &(cells[recv_cells[r_dir].e[c]].pList);
      for(n=0; n< pl->n; n++) {
	for(i=0;i<3;i++) pl->part[n].r.p[i] = recv_buf.e[g+i];
	g += 3;
      }
    }
  }
}

void collect_ghost_forces()
{
  int s_dir, r_dir;
  int c, n, g, i;
  ParticleList *pl;

  /* GHOST_TRACE(fprintf(stderr,"%d: collect_ghost_forces:\n",this_node)); */

#ifdef GHOST_FORCE_DEBUG
  {
    int m,o;
    INNER_CELLS_LOOP(m, n, o) {
      pl = &(CELL_PTR(m, n, o)->pList);
      for(i=0;i<pl->n;i++) {
	GHOST_FORCE_TRACE(fprintf(stderr,"%d: Collect_forces: P %d: home_force (%.3e,%.3e,%.3e)\n",
			 this_node,pl->part[i].r.identity,pl->part[i].f[0],pl->part[i].f[1],pl->part[i].f[2]));
      }
    }
  }
#endif


  for(s_dir=5; s_dir>=0; s_dir--) {         /* direction loop backward */
    if(s_dir%2 == 0) r_dir = s_dir+1;
    else             r_dir = s_dir-1;

    /* loop recv cells - copy forces to buffer*/
    g=0;
    for(c=0; c<recv_cells[r_dir].n; c++) {
      pl = &(cells[recv_cells[r_dir].e[c]].pList);
      for(n=0; n< pl->n; n++) {
	for(i=0;i<3;i++) send_buf.e[g+i] = pl->part[n].f[i];
	g += 3;
      }
    }
    /* send forces */
    send_posforce(r_dir, ghost_recv_size[r_dir], ghost_send_size[s_dir]);
    /* loop send cells - add buffer forces to local forces */
    g=0;
    for(c=0; c<send_cells[s_dir].n; c++) {
      pl = &(cells[send_cells[s_dir].e[c]].pList);
      for(n=0;n< pl->n;n++) {
	for(i=0; i<3; i++) pl->part[n].f[i] += recv_buf.e[g+i];
	g += 3;
      }
    } 
  }

#ifdef GHOST_FORCE_DEBUG
  {
    int m,o;
    INNER_CELLS_LOOP(m, n, o) {
      pl = &(CELL_PTR(m, n, o)->pList);
      for(i=0;i<pl->n;i++) {
	GHOST_FORCE_TRACE(fprintf(stderr,"%d: Collect_forces: P %d: tot_force (%.3e,%.3e,%.3e)\n",
			 this_node,pl->part[i].r.identity,pl->part[i].f[0],pl->part[i].f[1],pl->part[i].f[2]));
      }
    }
  }
#endif

}

/*******************  privat functions  *******************/

/** Creates an linear index list of a sub grid.
    The sub grid is defined by its lower and upper corner:\\
    from (lc[0],lc[1],lc[2]) to (hc[0],hc[1],hc[2])\\
    The grid dimension is given with (gs[0],gs[1],gs[2])\\
    The linear index list of length <returnvalue> is stored 
    in list starting from position start. max should be the 
    total length of list to ensure that the indices fit into list.
 */  
int sub_grid_indices(int* list, int start, int max,
		     int lc[3], int hc[3], int gs[3])
{
  int i,size,p0,p1,p2;
  /* sanity check */
  for(i=0;i<3;i++) {
    if(lc[i]<0 || lc[i] >= gs[i]) return 0;
    if(hc[i]<0 || hc[i] >= gs[i]) return 0;
    if(lc[i] > hc[i]) return 0;
  }

  size = (hc[0]+1-lc[0]);
  for(i=1;i<3;i++) size *= (hc[i]+1-lc[i]);
  /* check array size */
  if(size+start>max) return -1;

  i=start;
  for(p0=lc[0];p0<=hc[0];p0++)
    for(p1=lc[1];p1<=hc[1];p1++)
      for(p2=lc[2];p2<=hc[2];p2++) {
	if(i>max) fprintf(stderr,"%d: sub_grid_indices: Array overflow: %d>%d\n",this_node,i,max); 
	list[i] = get_linear_index(p0,p1,p2,gs);
	i++;
      }

  return size;
}

int move_to_p_buf(ParticleList *pl, int ind)
{
  int bonds;

  GHOST_TRACE(fprintf(stderr,"%d: move_to_p_buf(List,%d): Move particle %d with %d bonds\n",
		      this_node,ind,pl->part[ind].r.identity,pl->part[ind].bl.n));
    
  /* check bond  buffer sizes */
  bonds = pl->part[ind].bl.n;
  if( ((b_send_buf.n + bonds +1) >= b_send_buf.max) ) 
    realloc_intlist(&b_send_buf, b_send_buf.n + bonds+1 );

  /* copy bond information to b_send_buf */
  /* store bonds array size */
  b_send_buf.e[b_send_buf.n] = bonds;
  b_send_buf.n += 1;
  if(bonds>0) {
    memcpy(&(b_send_buf.e[b_send_buf.n]), pl->part[ind].bl.e, bonds*sizeof(int));
    b_send_buf.n += bonds;
    realloc_intlist(&(pl->part[ind].bl), 0);
  }

  /* delete it from local_particles list and move particle to p_send_buf */
  local_particles[pl->part[ind].r.identity] = NULL;
  move_unindexed_particle(&p_send_buf, pl, ind);

  GHOST_TRACE(fprintf(stderr,"%d: now: p_send_buf.n = %d, b_send_buf.n = %d\n",
		      this_node,p_send_buf.n,b_send_buf.n));


  if(ind < (pl->n)) ind--;
  return ind;
}

void send_particles(int s_dir)
{
  int evenodd;
  int r_dir;
  int send_sizes[2],recv_sizes[2];
  MPI_Status status;

  /* GHOST_TRACE(fprintf(stderr,"%d: send_particles(%d)\n",this_node,s_dir)); */

  /* check if communication goes to the very same node */
  if(node_neighbors[s_dir] != this_node) {
    send_sizes[0] = p_send_buf.n;
    send_sizes[1] = b_send_buf.n;
    /* calc recv direction (r_dir) from send direction (s_dir) */
    if(s_dir%2 == 0) r_dir = s_dir+1;
    else             r_dir = s_dir-1;
    /* two step communication: first all even positions than all odd */
    for(evenodd=0; evenodd<2;evenodd++) {
      if((node_pos[s_dir/2]+evenodd)%2==0) {
	GHOST_TRACE(if(p_send_buf.n>0) 
		    fprintf(stderr,"%d: send_part(%d): Send %d part to node %d\n"
			    ,this_node,s_dir,p_send_buf.n,node_neighbors[s_dir]));
	MPI_Send(send_sizes,2,MPI_INT,node_neighbors[s_dir],
		 REQ_SEND_PART ,MPI_COMM_WORLD);
	if(p_send_buf.n>0)
	  MPI_Send(p_send_buf.part,p_send_buf.n*sizeof(Particle), MPI_BYTE, 
		   node_neighbors[s_dir], REQ_SEND_PART, MPI_COMM_WORLD);
	if(b_send_buf.n>0)
	  MPI_Send(b_send_buf.e, b_send_buf.n, MPI_INT, node_neighbors[s_dir], 
		   REQ_SEND_PART, MPI_COMM_WORLD);
      }
      else {
	MPI_Recv(recv_sizes, 2, MPI_INT, node_neighbors[r_dir], 
		 REQ_SEND_PART, MPI_COMM_WORLD, &status);
	p_recv_buf.n = recv_sizes[0];
	b_recv_buf.n = recv_sizes[1];
	if(p_recv_buf.n>0) {
	  if(p_recv_buf.n >= p_recv_buf.max) 
	    realloc_particles(&p_recv_buf, p_recv_buf.n);
	  MPI_Recv(p_recv_buf.part,p_recv_buf.n*sizeof(Particle), MPI_BYTE,
		   node_neighbors[r_dir],REQ_SEND_PART,MPI_COMM_WORLD,&status);
	}
	if(b_recv_buf.n>0) { 
	  if(b_recv_buf.n >= b_recv_buf.max) 
	    realloc_intlist(&b_recv_buf, b_recv_buf.n);
	  MPI_Recv(b_recv_buf.e,b_recv_buf.n, MPI_INT, node_neighbors[r_dir],
		   REQ_SEND_PART,MPI_COMM_WORLD,&status);
	}
	GHOST_TRACE(if(p_recv_buf.n>0) 
		    fprintf(stderr,"%d: send_part(%d): Recv %d part from node %d (r_dir=%d)\n",
			    this_node,s_dir,p_recv_buf.n,node_neighbors[r_dir],r_dir));
      }
    }
  }
  else {                 /* communication goes to the same node! */ 
    fprintf(stderr,"%d: send_particles to same node should not happen\n",this_node);
    errexit();
  }
}

void append_particles(int dir)
{
  int i, c_ind, b_ind=0;
  ParticleList *pl;
  Particle *part;
  
  for(i=0; i<p_recv_buf.n; i++) {
    fold_coordinate(p_recv_buf.part[i].r.p, p_recv_buf.part[i].i, dir);
    c_ind = pos_to_capped_cell_grid_ind(p_recv_buf.part[i].r.p);
    pl = &(cells[c_ind].pList);
    GHOST_TRACE(fprintf(stderr,"%d: append part id=%d, pos=(%.3f,%.3f,%.3f) to cell %d\n",
			this_node, p_recv_buf.part[i].r.identity, p_recv_buf.part[i].r.p[0],
			p_recv_buf.part[i].r.p[1], p_recv_buf.part[i].r.p[2], c_ind));
    part = append_unindexed_particle(pl, &(p_recv_buf.part[i]));
    part->bl.n = b_recv_buf.e[b_ind];
    b_ind++;
    realloc_intlist(&(part->bl),part->bl.n);
    if(part->bl.n > 0) { 
      memcpy(part->bl.e,&(b_recv_buf.e[b_ind]), part->bl.n*sizeof(int));
      b_ind += part->bl.n;
    }
  }
}

void send_ghosts(int s_dir)
{
  int evenodd;
  int r_dir=0;
  MPI_Status status;
  int tmp;
  ReducedParticle *tmp_gp;
  int *tmp_ip;

  /* calc recv direction (r_dir) from send direction (s_dir) */
  if(s_dir%2 == 0) r_dir = s_dir+1;
  else             r_dir = s_dir-1;

  /* check if communication goes to the very same node */
  if(node_neighbors[s_dir] != this_node) {
    /* two step communication: first all even positions than all odd */
    for(evenodd=0; evenodd<2; evenodd++) {
      if((node_pos[s_dir/2]+evenodd)%2==0) {
	MPI_Send(n_send_ghosts[s_dir].e, n_send_ghosts[s_dir].n, MPI_INT,
		 node_neighbors[s_dir], REQ_SEND_GHOSTS, MPI_COMM_WORLD);

	ghost_send_size[s_dir] = n_send_ghosts[s_dir].e[send_cells[s_dir].n];
  
	MPI_Send(g_send_buf.part, ghost_send_size[s_dir]*sizeof(ReducedParticle), MPI_BYTE, 
		 node_neighbors[s_dir], REQ_SEND_GHOSTS, MPI_COMM_WORLD);
      }
      else {
	MPI_Recv(n_recv_ghosts[r_dir].e, n_recv_ghosts[r_dir].n, MPI_INT,
		 node_neighbors[r_dir],REQ_SEND_GHOSTS,MPI_COMM_WORLD,&status);

	ghost_recv_size[r_dir] = n_recv_ghosts[r_dir].e[recv_cells[r_dir].n];
	if(ghost_recv_size[r_dir] > g_recv_buf.max) 
	  realloc_redParticles(&g_recv_buf, ghost_recv_size[r_dir]);

	MPI_Recv(g_recv_buf.part, ghost_recv_size[r_dir]*sizeof(ReducedParticle), MPI_BYTE,
		 node_neighbors[r_dir],REQ_SEND_GHOSTS,MPI_COMM_WORLD,&status);
      }
    }
  }
  else {                 /* communication goes to the same node! */ 

    ghost_send_size[s_dir] = n_send_ghosts[s_dir].e[send_cells[s_dir].n];
 
    tmp_ip                    = n_send_ghosts[s_dir].e;
    n_send_ghosts[s_dir].e    = n_recv_ghosts[r_dir].e;
    n_recv_ghosts[r_dir].e    = tmp_ip;
    tmp                       = n_send_ghosts[s_dir].max;
    n_send_ghosts[s_dir].max  = n_recv_ghosts[r_dir].max;
    n_recv_ghosts[r_dir].max  = tmp;
    tmp                       = n_send_ghosts[s_dir].n;
    n_send_ghosts[s_dir].n    = n_recv_ghosts[r_dir].n;
    n_recv_ghosts[r_dir].n    = tmp;

    ghost_recv_size[r_dir] = n_recv_ghosts[r_dir].e[recv_cells[r_dir].n];
    if(ghost_recv_size[r_dir] > g_recv_buf.max) 
      realloc_redParticles(&g_recv_buf, ghost_recv_size[r_dir]);

    tmp_gp          = g_send_buf.part;
    g_send_buf.part = g_recv_buf.part; 
    g_recv_buf.part = tmp_gp;
    tmp             = g_send_buf.max;
    g_send_buf.max  = g_recv_buf.max; 
    g_recv_buf.max  = tmp;
    tmp             = g_send_buf.n;
    g_send_buf.n    = g_recv_buf.n; 
    g_recv_buf.n    = tmp;
  }
}

void send_posforce(int s_dir, int send_size, int recv_size)
{
  int evenodd;
  int r_dir;
  MPI_Status status;

  /* check if communication goes to the very same node */
  if(node_neighbors[s_dir] != this_node) {
    /* calc recv direction (r_dir) from send direction (s_dir) */
    if(s_dir%2 == 0) r_dir = s_dir+1;
    else             r_dir = s_dir-1;
    /* two step communication: first all even positions than all odd */
    for(evenodd=0; evenodd<2;evenodd++) {
      if((node_pos[s_dir/2]+evenodd)%2==0) 
	MPI_Send(send_buf.e, 3*send_size, MPI_DOUBLE, 
		 node_neighbors[s_dir],REQ_SEND_POS,MPI_COMM_WORLD);
      else 
	MPI_Recv(recv_buf.e, 3*recv_size, MPI_DOUBLE,
		 node_neighbors[r_dir],REQ_SEND_POS,MPI_COMM_WORLD,&status);    
    }
  }
  else {                  /* communication goes to the same node! */ 
    double *tmp_dp;
    int tmp;

    tmp_dp     = send_buf.e;
    send_buf.e = recv_buf.e;
    recv_buf.e = tmp_dp;  

    tmp          = send_buf.max;
    send_buf.max = recv_buf.max;
    recv_buf.max = tmp;
  }
}

/** \file forces.c Force calculation.
 *
 *  For more information see \ref forces.h "forces.h".
*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "config.h"
#include "forces.h"
#include "debug.h"
#include "thermostat.h"
#include "communication.h"
#include "ghosts.h" 
#include "verlet.h"
#include "utils.h"
#include "interaction_data.h"
#include "grid.h"
#include "p3m.h"
#include "cells.h"
/* include the force files */
#include "lj.h"
#include "fene.h"
#include "angle.h"

double minimum_part_dist = -1;

/** initialize real particle forces with thermostat forces and
    ghost particle forces with zero. */
void init_forces();

/************************************************************/

void force_init()
{
  FORCE_TRACE(fprintf(stderr,"%d: force_init:\n",this_node));
  FORCE_TRACE(fprintf(stderr,"%d: found %d interaction types\n",
		      this_node,n_interaction_types));
  FORCE_TRACE(fprintf(stderr,"%d: found %d particles types\n",
		      this_node,n_particle_types));
  FORCE_TRACE(fprintf(stderr,"%d: lj_force_cap = %f\n",this_node,lj_force_cap));

  calc_lj_cap_radii(lj_force_cap);

}


/************************************************************/

static Particle *checked_particle_ptr(int id)
{
  Particle *p = local_particles[id];
  if(!p) {
    fprintf(stderr,"%d: ERROR: Atom %d has bond to unknown particle "
	    "(probably on different node)\n",this_node, id); 
    errexit();
  }
  return p;
}

/************************************************************/

void force_calc()
{
  Cell *cell;
  Particle *p, **pairs;
  Particle *p1, *p2;
  int k, i,j, m, n, o, np;
  double d[3], dist2, dist;
  IA_parameters *ia_params;
  /* bonded interactions */
  int type_num;

  /* preparation */
  minimum_part_dist = box_l[0] + box_l[1] + box_l[2];
  init_forces();

  /* force calculation loop. */
  INNER_CELLS_LOOP(m, n, o) {
    cell = CELL_PTR(m, n, o);
    p  = cell->pList.part;
    np = cell->pList.n;

    /* calculate bonded interactions (loop local particles) */
    for(j = 0; j < np; j++) {
      p1 = &p[j];
      i=0;
      while(i<p1->bl.n) {
	type_num = p1->bl.e[i];
	switch(bonded_ia_params[type_num].type) {
	case BONDED_IA_FENE:
	  add_fene_pair_force(p1,
			      checked_particle_ptr(p1->bl.e[i+1]), type_num);
	  i+=2; break;
	case BONDED_IA_ANGLE:
	  add_angle_pair_force(p1,
			       checked_particle_ptr(p1->bl.e[i+1]),
			       checked_particle_ptr(p1->bl.e[i+2]), type_num);
	  i+=3; break;
	default :
	  fprintf(stderr,"WARNING: Bonds of atom %d unknown\n",p1->r.identity);
	  i = p1->bl.n; 
	  break;
	}
      }
    }

    /* calculate non bonded interactions (loop verlet lists of neighbors) */
    for (k = 0; k < cell->n_neighbors; k++) {
      pairs = cell->nList[k].vList.pair;  /* verlet list */
      np    = cell->nList[k].vList.n;     /* length of verlet list */

      /* verlet list loop */
      for(i=0; i<2*np; i+=2) {
	p1 = pairs[i];                    /* pointer to particle 1 */
	p2 = pairs[i+1];                  /* pointer to particle 2 */
	ia_params = get_ia_param(p1->r.type,p2->r.type);
	/* distance calculation */
	for(j=0; j<3; j++) d[j] = p1->r.p[j] - p2->r.p[j];
	dist2 = SQR(d[0]) + SQR(d[1]) + SQR(d[2]);
	dist  = sqrt(dist2);

	/* lennnard jones */
	add_lj_pair_force(p1,p2,ia_params,d,dist);

	/* real space coulomb */
	add_coulomb_pair_force(p1,p2,d,dist2,dist);

	/* minimal particle distance calculation */
	if (dist < minimum_part_dist)
	  minimum_part_dist = dist;
      } 
    }
  }

  /* calculate k-space part of electrostatic interaction. */
  if(p3m.bjerrum != 0.0) P3M_calc_kspace_forces();
}

/************************************************************/

void init_forces()
{
  Particle *p;
  int np, m, n, o, i;
  /* initialize forces with thermostat forces and
     ghost forces with zero */
  CELLS_LOOP(m, n, o) {
    p  = CELL_PTR(m, n, o)->pList.part;
    np = CELL_PTR(m, n, o)->pList.n;
    /* ghost particle selection */
    if (m == 0 || m == ghost_cell_grid[0] - 1 ||
	n == 0 || n == ghost_cell_grid[1] - 1 ||
	o == 0 || o == ghost_cell_grid[2] - 1) {
      for (i = 0; i < np; i++) {
	p[i].f[0] = 0;
	p[i].f[1] = 0;
	p[i].f[2] = 0;
      }
    }
    /* real particle selection */
    else {
      for (i = 0; i < np; i++)
	friction_thermo(&p[i]);
    }
  }
}

void force_exit()
{
  FORCE_TRACE(fprintf(stderr,"%d: force_exit:\n",this_node));
}



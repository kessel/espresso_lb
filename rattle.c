// This file is part of the ESPResSo distribution (http://www.espresso.mpg.de).
// It is therefore subject to the ESPResSo license agreement which you accepted upon receiving the distribution
// and by which you are legally bound while utilizing this file in any form or way.
// There is NO WARRANTY, not even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// You should have received a copy of that license along with this program;
// if not, refer to http://www.espresso.mpg.de/license.html where its current version can be found, or
// write to Max-Planck-Institute for Polymer Research, Theory Group, PO Box 3148, 55021 Mainz, Germany.
// Copyright (c) 2002-2004; all rights reserved unless otherwise stated.

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "domain_decomposition.h"
#include "rattle.h"

#ifdef BOND_CONSTRAINT

/*Initialize old positions (particle positions at previous time step)
  of the particles*/
void save_old_pos()
{
  int c, i, j, np;
  Particle *p;
  Cell *cell;
  for (c = 0; c < local_cells.n; c++)
  {
    cell = local_cells.cell[c];
    p  = cell->part;
    np = cell->n;
    for(i = 0; i < np; i++) {
      for(j=0;j<3;j++)
        p[i].r.p_old[j]=p[i].r.p[j];
    } //for i loop
  }// for c loop

  for (c = 0; c < ghost_cells.n; c++)
  {
    cell = ghost_cells.cell[c];
    p  = cell->part;
    np = cell->n;
    for(i = 0; i < np; i++) {
       for(j=0;j<3;j++)
          p[i].r.p_old[j]=p[i].r.p[j];
    }
  }
}

/**Initialize the correction vector. The correction vector is stored in f.f of particle strcuture. */
void init_correction_vector()
{

  int c, i, j, np;
  Particle *p;
  Cell *cell;

  for (c = 0; c < local_cells.n; c++)
  {
    cell = local_cells.cell[c];
    p  = cell->part;
    np = cell->n;
    for(i = 0; i < np; i++) {
      for(j=0;j<3;j++)
        p[i].f.f[j] = 0.0;
     } //for i loop
  }// for c loop

  for (c = 0; c < ghost_cells.n; c++)
  {
    cell = ghost_cells.cell[c];
    p  = cell->part;
    np = cell->n;
    for(i = 0; i < np; i++) {
       for(j=0;j<3;j++)
          p[i].f.f[j] = 0.0;
    }
  }
}

/**Compute positional corrections*/
void compute_pos_corr_vec()
{
   int i, j, k, c, np;
   Cell *cell;
   Particle *p, *p1, *p2;
   double r_ij_t[3], r_ij[3], r_ij_dot, G, pos_corr;
   /*
   #ifdef NPT
     double scale = 0.5*time_step*time_step;
   #endif
   */

     for (c = 0; c < local_cells.n; c++)
     {
      cell = local_cells.cell[c];
      p  = cell->part;
      np = cell->n;
      for(i = 0; i < np; i++) {
        p1 = &p[i];
        k=0;
	while(k<p1->bl.n)
	{
	  if( bonded_ia_params[p1->bl.e[k]].type == BONDED_IA_RIGID_BOND )
          {
	    p2 = checked_particle_ptr(p1->bl.e[k+1]);
            get_mi_vector(r_ij_t, p1->r.p_old, p2->r.p_old);
            get_mi_vector(r_ij  , p1->r.p    , p2->r.p    );
	    r_ij_dot = scalar(r_ij_t, r_ij);
	    G = 0.50*(bonded_ia_params[p1->bl.e[k]].p.rigid_bond.d2 - sqrlen(r_ij))/r_ij_dot;
	    G /= (p1->p.mass+p2->p.mass);
            for (j=0;j<3;j++) {
	        pos_corr = G*r_ij_t[j];
	        p1->f.f[j] += pos_corr*p2->p.mass;
                p2->f.f[j] -= pos_corr*p1->p.mass;

	    }
	  }
	  k+=(bonded_ia_params[p1->bl.e[k]].num+1);
	} //while loop
       } //for i loop
     } //for c loop
}

/**Apply corrections to each particle and check if Verlet list is required to be rebuilt**/
void app_correction_check_VL_rebuild()
{
  int c, i, j, np;
  Particle *p, *p1;
  Cell *cell;
  double skin2 = SQR(skin/2.0);

  rebuild_verletlist=0;
     /*Apply corrections*/
     for (c = 0; c < local_cells.n; c++)
     {
      cell = local_cells.cell[c];
      p  = cell->part;
      np = cell->n;
      for(i = 0; i < np; i++) {
        p1 = &p[i];
	for (j=0;j<3;j++) {
	   p1->r.p[j] += p1->f.f[j];
	   p1->m.v[j] += p1->f.f[j];
	}
	/* Verlet criterion check */
	if(distance2(p1->r.p, p1->l.p_old) > skin2 ) rebuild_verletlist = 1;

	/**Completed for one particle*/
       } //for i loop
     } //for c loop
}


/**Check if further iterations are required to satisfy the specified tolerance.*/
int check_tol_pos()
{
 int i, k, c, np;
 int repeat = 0;
 Cell *cell;
 Particle *p;
 Bonded_ia_parameters *b_ia;
 double r_ij[3], tol;
 for (c = 0; c < local_cells.n; c++) {
    cell = local_cells.cell[c];
    p  = cell->part;
    np = cell->n;
    for(i = 0; i < np; i++) {
       k=0;
       while(k<p[i].bl.n)
       {
             b_ia = &bonded_ia_params[p[i].bl.e[k]];
	     if(b_ia->type == BONDED_IA_RIGID_BOND)
             {
	        get_mi_vector(r_ij, p[i].r.p , checked_particle_ptr(p[i].bl.e[k+1])->r.p);
		tol = fabs(0.5*(b_ia->p.rigid_bond.d2 - sqrlen(r_ij))/b_ia->p.rigid_bond.d2);
		repeat = repeat || (tol > b_ia->p.rigid_bond.p_tol);
             }
	     k += (b_ia->num+1);
	} //while k loop
     } //for i loop
  }// for c loop
  return(repeat);
}


void correct_pos_shake()
{
   int    repeat_,  cnt=0;
   int repeat=1;

   while (repeat && cnt<500)
   {
     init_correction_vector();
     repeat_ = 0;
     compute_pos_corr_vec();
     ghost_communicator(&cell_structure.collect_ghost_force_comm);
     app_correction_check_VL_rebuild();
     /**Ghost Positions Update*/
     ghost_communicator(&cell_structure.update_ghost_pos_comm);
     /**Calculate latest bond distances, determine the tolerance and check if iteration is required*/
     repeat_ = check_tol_pos();
     if(this_node==0)
         MPI_Reduce(&repeat_, &repeat, 1, MPI_INT, MPI_LOR, 0, MPI_COMM_WORLD);
     else
         MPI_Reduce(&repeat_, NULL, 1, MPI_INT, MPI_LOR, 0, MPI_COMM_WORLD);
     MPI_Bcast(&repeat, 1, MPI_INT, 0, MPI_COMM_WORLD);
     if(!repeat)
        anounce_rebuild_vlist();
       cnt++;
   }// while(repeat) loop
   if (cnt >= 500) {
         fprintf(stderr, "%d: RATTLE failed to converge after %d iterations !!\n", this_node, cnt);
	 errexit();
   }
}

/**The forces are transfered temporarily from f.f member of particle structure to r.p_old,
    which is idle now and initialize the velocity correction vector to zero at f.f[3]
    of Particle structure*/
void transfer_force_init_vel()
{

  int c, i, j, np;
  Particle *p;
  Cell *cell;

  for (c = 0; c < local_cells.n; c++)
  {
    cell = local_cells.cell[c];
    p  = cell->part;
    np = cell->n;
    for(i = 0; i < np; i++) {
      for(j=0;j<3;j++)
      {
        p[i].r.p_old[j]=p[i].f.f[j];
	p[i].f.f[j]=0.0;
      }
     } //for i loop
  }// for c loop

  for (c = 0; c < ghost_cells.n; c++)
  {
    cell = ghost_cells.cell[c];
    p  = cell->part;
    np = cell->n;
    for(i = 0; i < np; i++) {
       for(j=0;j<3;j++)
       {
          p[i].r.p_old[j]=p[i].f.f[j];
	  p[i].f.f[j]=0.0;
       }

    }
  }
}

void compute_vel_corr_vec()
{

   int i, j, k, c, np;
   Cell *cell;
   Particle *p, *p1, *p2;
   double v_ij[3], r_ij[3], K, vel_corr;

   for (c = 0; c < local_cells.n; c++)
     {
      cell = local_cells.cell[c];
      p  = cell->part;
      np = cell->n;
      for(i = 0; i < np; i++) {
        p1 = &p[i];
        k=0;
	while(k<p1->bl.n)
	{
	  if( bonded_ia_params[p1->bl.e[k]].type == BONDED_IA_RIGID_BOND )
          {
	    p2 = checked_particle_ptr(p1->bl.e[k+1]);
	    vector_subt(v_ij, p1->m.v, p2->m.v);
            get_mi_vector(r_ij, p1->r.p, p2->r.p);
            K = scalar(v_ij, r_ij)/bonded_ia_params[p1->bl.e[k]].p.rigid_bond.d2;
	    K /= (p1->p.mass + p2->p.mass);

            for (j=0;j<3;j++)
	    {
	        vel_corr = K*r_ij[j];
	        p1->f.f[j] -= vel_corr*p2->p.mass;
		p2->f.f[j] += vel_corr*p1->p.mass;

	    }
	  }
	  k+=(bonded_ia_params[p1->bl.e[k]].num+1);
	} //while loop
       } //for i loop
     } //for c loop


}

/**Apply velocity corrections*/
void apply_vel_corr()
{

  int c, i, j, np;
  Particle *p, *p1;
  Cell *cell;

     /*Apply corrections*/
     for (c = 0; c < local_cells.n; c++)
     {
      cell = local_cells.cell[c];
      p  = cell->part;
      np = cell->n;
      for(i = 0; i < np; i++) {
        p1 = &p[i];
	for (j=0;j<3;j++) {
	   p1->m.v[j] += p1->f.f[j];
	   //p1->r.p_old[j] += p1->f.f[j];
	}
	/**Completed for one particle*/
       } //for i loop
     } //for c loop

}

/**Check if tolerance for convergence of velocity corrections is fulfilled*/
int check_tol_vel()
{
 int i, k, c, np;
 int repeat = 0;
 Cell *cell;
 Particle *p;
 Bonded_ia_parameters *b_ia;
 double r_ij[3], v_ij[3], tol;

 for (c = 0; c < local_cells.n; c++) {
    cell = local_cells.cell[c];
    p  = cell->part;
    np = cell->n;
    for(i = 0; i < np; i++) {
       k=0;
       while(k<p[i].bl.n)
       {
             b_ia = &bonded_ia_params[p[i].bl.e[k]];
	     if(b_ia->type == BONDED_IA_RIGID_BOND)
             {
	        get_mi_vector(r_ij, p[i].r.p , checked_particle_ptr(p[i].bl.e[k+1])->r.p);
		vector_subt(v_ij, p[i].m.v , checked_particle_ptr(p[i].bl.e[k+1])->m.v);
		tol = fabs(scalar(r_ij, v_ij));
		repeat = repeat || (tol > b_ia->p.rigid_bond.v_tol);
             }
	     k += (b_ia->num+1);
	} //while k loop
     } //for i loop
  }// for c loop
  return(repeat);
}

/**Put back the forces from r.p_old to f.f*/
void revert_force()
{
  int c, i, j, np;
  Particle *p;
  Cell *cell;
  for (c = 0; c < local_cells.n; c++)
  {
    cell = local_cells.cell[c];
    p  = cell->part;
    np = cell->n;
    for(i = 0; i < np; i++) {
      for(j=0;j<3;j++)
      	p[i].f.f[j]=p[i].r.p_old[j];

     } //for i loop
  }// for c loop


  for (c = 0; c < ghost_cells.n; c++)
  {
    cell = ghost_cells.cell[c];
    p  = cell->part;
    np = cell->n;
    for(i = 0; i < np; i++)
    {
       for(j=0;j<3;j++)
  	  p[i].f.f[j]=p[i].r.p_old[j];
    }
  }
}

void correct_vel_shake()
{
   int    repeat_, repeat=1, cnt=0;
   /**transfer the current forces to r.p_old of the particle structure so that
   velocity corrections can be stored temporarily at the f.f[3] of the particle
   structure  */
   transfer_force_init_vel();
   while (repeat && cnt<500)
   {
     init_correction_vector();
     compute_vel_corr_vec();
     ghost_communicator(&cell_structure.collect_ghost_force_comm);
     apply_vel_corr();
     ghost_communicator(&cell_structure.update_ghost_pos_comm);
     repeat_ = check_tol_vel();
     if(this_node == 0)
       MPI_Reduce(&repeat_, &repeat, 1, MPI_INT, MPI_LOR, 0, MPI_COMM_WORLD);
     else
       MPI_Reduce(&repeat_, NULL, 1, MPI_INT, MPI_LOR, 0, MPI_COMM_WORLD);

     MPI_Bcast(&repeat, 1, MPI_INT, 0, MPI_COMM_WORLD);
     cnt++;
   }

   if (cnt >= 500) {
         fprintf(stderr, "%d: VEL CORRECTIONS IN RATTLE failed to converge after %d iterations !!\n", this_node, cnt);
	 errexit();
   }
   /**Puts back the forces from r.p_old to f.f[3]*/
   revert_force();
}


void print_bond_len()
{
 int i, k, c, np;
 Cell *cell;
 Particle *p;
 Bonded_ia_parameters *b_ia;
 double r_ij[3];
 printf("%d: ", this_node);
 for (c = 0; c < local_cells.n; c++) {
    cell = local_cells.cell[c];
    p  = cell->part;
    np = cell->n;
    for(i = 0; i < np; i++) {
       k=0;
       while(k<p[i].bl.n)
       {
             b_ia = &bonded_ia_params[p[i].bl.e[k]];
	     if(b_ia->type == BONDED_IA_RIGID_BOND)
             {
	        get_mi_vector(r_ij, p[i].r.p , checked_particle_ptr(p[i].bl.e[k+1])->r.p);
		printf(" bl (%d %d): %f\t", p[i].p.identity, checked_particle_ptr(p[i].bl.e[k+1])->p.identity,sqrlen(r_ij));

             }
	     k += (b_ia->num+1);
	} //while k loop
     } //for i loop
  }// for c loop
  printf("\n");
}

#endif

#ifndef DEBYE_HUECKEL_H
#define DEBYE_HUECKEL_H
/** \file debye_hueckel.h
 *  Routines to calculate the Debye_Hueckel  Energy or/and Debye_Hueckel force 
 *  for a particle pair.
 *  \ref forces.c
 *
 *  <b>Responsible:</b>
 *  <a href="mailto:limbach@mpip-mainz.mpg.de">Hanjo</a>
*/


/** \name Functions */
/************************************************************/
/*@{*/

/** Computes the Debye_Hueckel pair force and adds this
    force to the particle forces (see \ref #inter). 
    @param p1        Pointer to first particle.
    @param p2        Pointer to second/middle particle.
    @param d         Vector pointing from p1 to p2.
    @param dist      Distance between p1 and p2.
*/
MDINLINE void add_dh_coulomb_pair_force(Particle *p1, Particle *p2, double d[3], double dist)
{
  int j;
  double kappa_dist, fac;
  
  if(dist < dh_params.r_cut) {
    if(dh_params.kappa > 0.0) {
      /* debye hueckel case: */
      kappa_dist = dh_params.kappa*dist;
      fac = dh_params.prefac * (exp(-kappa_dist)/(dist*dist*dist)) * (1.0 + kappa_dist);
    }
    else {
      /* pure coulomb case: */
      fac = dh_params.prefac / (dist*dist*dist);
    }
    for(j=0;j<3;j++) {
      p1->f[j] += fac * d[j];
      p2->f[j] -= fac * d[j];
    }
    ONEPART_TRACE(if(p1->r.identity==check_id) fprintf(stderr,"%d: OPT: DH   f = (%.3e,%.3e,%.3e) with part id=%d at dist %f fac %.3e\n",this_node,p1->f[0],p1->f[1],p1->f[2],p2->r.identity,dist,fac));
    ONEPART_TRACE(if(p2->r.identity==check_id) fprintf(stderr,"%d: OPT: DH   f = (%.3e,%.3e,%.3e) with part id=%d at dist %f fac %.3e\n",this_node,p2->f[0],p2->f[1],p2->f[2],p1->r.identity,dist,fac));    
  }
}

MDINLINE double dh_coulomb_pair_energy(Particle *p1, Particle *p2, double dist)
{
  if(dist < dh_params.r_cut) {
    if(dh_params.kappa > 0.0)
      return dh_params.prefac * exp(-dh_params.kappa*dist) / dist;
    else 
      return dh_params.prefac / dist;
  }
  return 0.0;
}

/*@}*/

#endif

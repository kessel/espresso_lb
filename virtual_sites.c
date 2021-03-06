// This file is part of the ESPResSo distribution (http://www.espresso.mpg.de).
// It is therefore subject to the ESPResSo license agreement which you accepted upon receiving the distribution
// and by which you are legally bound while utilizing this file in any form or way.
// There is NO WARRANTY, not even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// You should have received a copy of that license along with this program;
// if not, refer to http://www.espresso.mpg.de/license.html where its current version can be found, or
// write to Max-Planck-Institute for Polymer Research, Theory Group, PO Box 3148, 55021 Mainz, Germany.
// Copyright (c) 2002-2009; all rights reserved unless otherwise stated.

#include "virtual_sites.h"
#include "pressure.h"

/** \file virtual_sites.h
 *  This file contains routine to handle virtual sites
 *  Virtual sites are like particles, but they will be not integrated.
 *  Step performed for virtual sites:
 *  - update virtual sites
 *  - calculate forces
 *  - distribute forces
 *  - move no-virtual particles
 *  - update virtual sites
 */

/** \name Private Functions */
#ifdef VIRTUAL_SITES
void update_mol_vel_particle(Particle *p);
void update_mol_pos_particle(Particle *p);

void calc_mol_vel(Particle *p_com,double v_com[3]);
void calc_mol_pos(Particle *p_com,double r_com[3]);
int calc_mol_pos_cfg(Particle *p_com,double r_com[3]);
void put_mol_force_on_parts(Particle *p_com);

void get_mol_dist_vector_cfg(Particle *p1,Particle *p2,double dist[3]);

void update_mol_vel_pos()
{
   //replace this by a better implementation later!
   update_mol_vel();
   update_mol_pos();
}

void update_mol_vel()
{
  Particle *p;
  int i, np, c;
  Cell *cell;
  for (c = 0; c < local_cells.n; c++) {
    cell = local_cells.cell[c];
    p  = cell->part;
    np = cell->n;
    for(i = 0; i < np; i++) {
       update_mol_vel_particle(&p[i]);
    }
  }
}

void update_mol_vel_particle(Particle *p){
   int j;
   double v_com[3];
   if (ifParticleIsVirtual(p)) {
      calc_mol_vel(p,v_com);
      for (j=0;j<3;j++){
         p->m.v[j] = v_com[j];
      }
   }
}

void update_mol_pos()
{
  Particle *p;
  int i, np, c;
  Cell *cell;
  for (c = 0; c < local_cells.n; c++) {
    cell = local_cells.cell[c];
    p  = cell->part;
    np = cell->n;
    for(i = 0; i < np; i++) {
       update_mol_pos_particle(&p[i]);
    }
    //only for real particles
  }
}

void update_mol_pos_particle(Particle *p){
   int j;
   double r_com[3];
   if (ifParticleIsVirtual(p)) {
      calc_mol_pos(p,r_com);
      for (j=0;j<3;j++){
         p->r.p[j] = r_com[j];
      }
   }
}

int update_mol_pos_cfg(){
  Particle *p;
  int i,j;
  double r_com[3];
  for(i=0; i<n_total_particles; i++) {
     p=&partCfg[i];
     if (ifParticleIsVirtual(p)){
         if (calc_mol_pos_cfg(p,r_com)==0){
            return 0;
         }
         for (j=0;j<3;j++){
            p->r.p[j] = r_com[j];
         }
     }
  }
  return 1;
}

void distribute_mol_force()
{
  Particle *p;
  int i, np, c;
  Cell *cell;
  for (c = 0; c < local_cells.n; c++) {
    cell = local_cells.cell[c];
    p  = cell->part;
    np = cell->n;
    for(i = 0; i < np; i++) {
      if (ifParticleIsVirtual(&p[i])) {
         if (sqrlen(p[i].f.f)!=0){
            put_mol_force_on_parts(&p[i]);
         }
      }
    }
  }
}

void calc_mol_vel(Particle *p_com,double v_com[3]){
   int i,j,mol_id;
   double M=0;
   Particle *p;
#ifdef VIRTUAL_SITES_DEBUG
   int count=0;
#endif
   for (i=0;i<3;i++){
      v_com[i]=0.0;
   }
   mol_id=p_com->p.mol_id;
   for (i=0;i<topology[mol_id].part.n;i++){
      p=local_particles[topology[mol_id].part.e[i]];
      #ifdef VIRTUAL_SITES_DEBUG
      if (p==NULL){
         char *errtxt = runtime_error(128 + 3*TCL_INTEGER_SPACE);
         ERROR_SPRINTF(errtxt,"Particle does not exist in calc_mol_vel! id=%i\n",topology[mol_id].part.e[i]);
         return;
      }
      #endif
      if (ifParticleIsVirtual(p)) continue;
      for (j=0;j<3;j++){
         v_com[j] += PMASS(*p)*p->m.v[j];
      }
      M+=PMASS(*p);
#ifdef VIRTUAL_SITES_DEBUG
      count++;
#endif
   }
   for (j=0;j<3;j++){
      v_com[j] /= M;
   }
#ifdef VIRTUAL_SITES_DEBUG
   if (count!=topology[mol_id].part.n-1){
      char *errtxt = runtime_error(128 + 3*TCL_INTEGER_SPACE);
      ERROR_SPRINTF(errtxt,"There is more than one COM in calc_mol_vel! mol_id=%i\n",mol_id);
      return;
   }
#endif
}

/* this is a local version of center of mass, because ghosts don't have image boxes*/
/* but p_com is a real particle */
void calc_mol_pos(Particle *p_com,double r_com[3]){
   int i,j,mol_id;
   double M=0;
   double vec12[3];
   Particle *p;
#ifdef VIRTUAL_SITES_DEBUG
   int count=0;
#endif
   for (i=0;i<3;i++){
      r_com[i]=0.0;
   }
   mol_id=p_com->p.mol_id;
   for (i=0;i<topology[mol_id].part.n;i++){
      p=local_particles[topology[mol_id].part.e[i]];
      #ifdef VIRTUAL_SITES_DEBUG
      if (p==NULL){
         char *errtxt = runtime_error(128 + 3*TCL_INTEGER_SPACE);
         ERROR_SPRINTF(errtxt,"Particle does not exist in calc_mol_pos! id=%i\n",topology[mol_id].part.e[i]);
         return;
      }
      #endif
      if (ifParticleIsVirtual(p)) continue;
      get_mi_vector(vec12,p->r.p, p_com->r.p);
      for (j=0;j<3;j++){
          r_com[j] += PMASS(*p)*vec12[j];
      }
      M+=PMASS(*p);
#ifdef VIRTUAL_SITES_DEBUG
      count++;
#endif
   }
   for (j=0;j<3;j++){
      r_com[j] /= M;
      r_com[j] += p_com->r.p[j];
   }
#ifdef VIRTUAL_SITES_DEBUG
   if (count!=topology[mol_id].part.n-1){
      char *errtxt = runtime_error(128 + 3*TCL_INTEGER_SPACE);
      ERROR_SPRINTF(errtxt,"There is more than one COM in calc_mol_pos! mol_id=%i\n",mol_id);
      return;
   }
#endif
}

int calc_mol_pos_cfg(Particle *p_com,double r_com[3]){
   int i,j,mol_id;
   double M=0;
   Particle *p;
#ifdef VIRTUAL_SITES_DEBUG
   int count=0;
#endif
   for (i=0;i<3;i++){
      r_com[i]=0.0;
   }
   mol_id=p_com->p.mol_id;
   for (i=0;i<topology[mol_id].part.n;i++){
      p=&partCfg[topology[mol_id].part.e[i]];
      if (ifParticleIsVirtual(p)) continue;
      for (j=0;j<3;j++){
            r_com[j] += PMASS(*p)*p->r.p[j];
      }
      M+=PMASS(*p);
#ifdef VIRTUAL_SITES_DEBUG
      count++;
#endif
   }
   for (j=0;j<3;j++){
      r_com[j] /= M;
   }
#ifdef VIRTUAL_SITES_DEBUG
   if (count!=topology[mol_id].part.n-1){
      fprintf(stderr,"There is more than one COM in calc_mol_pos_cfg! mol_id=%i\n",mol_id);
      return 0;
   }
#endif
   return 1;
}

void put_mol_force_on_parts(Particle *p_com){
   int i,j,mol_id;
   Particle *p;
   double force[3],M;
#ifdef VIRTUAL_SITES_DEBUG
   int count=0;
#endif
  mol_id=p_com->p.mol_id;
   for (i=0;i<3;i++){
      force[i]=p_com->f.f[i];
      p_com->f.f[i]=0.0;
   }
#ifdef MASS
   M=0;
   for (i=0;i<topology[mol_id].part.n;i++){
      p=local_particles[topology[mol_id].part.e[i]];
      #ifdef VIRTUAL_SITES_DEBUG
      if (p==NULL){
         char *errtxt = runtime_error(128 + 3*TCL_INTEGER_SPACE);
         ERROR_SPRINTF(errtxt,"Particle does not exist in put_mol_force_on_parts! id=%i\n",topology[mol_id].part.e[i]);
         return;
      }
      #endif
       if (ifParticleIsVirtual(p)) continue;
      M+=PMASS(*p);
   }
#else
   M=topology[mol_id].part.n-1;
#endif
   for (i=0;i<topology[mol_id].part.n;i++){
      p=local_particles[topology[mol_id].part.e[i]];
      #ifdef VIRTUAL_SITES_DEBUG
      if (p==NULL){
         char *errtxt = runtime_error(128 + 3*TCL_INTEGER_SPACE);
         ERROR_SPRINTF(errtxt,"Particle does not exist in put_mol_force_on_parts! id=%i\n",topology[mol_id].part.e[i]);
         return;
      }
      #endif
      if (!ifParticleIsVirtual(p)) {
         for (j=0;j<3;j++){
            p->f.f[j]+=PMASS(*p)*force[j]/M;
         }
#ifdef VIRTUAL_SITES_DEBUG
         count++;
#endif
      }
   }
#ifdef VIRTUAL_SITES_DEBUG
   if (count!=topology[mol_id].part.n-1){
      char *errtxt = runtime_error(128 + 3*TCL_INTEGER_SPACE);
      ERROR_SPRINTF(errtxt,"There is more than one COM input_mol_force_on_parts! mol_id=%i\n",mol_id);
      return;
   }
#endif
}

Particle *get_mol_com_particle(Particle *calling_p){
   int mol_id;
   int i;
   Particle *p;

   mol_id=calling_p->p.mol_id;
   for (i=0;i<topology[mol_id].part.n;i++){
      p=local_particles[topology[mol_id].part.e[i]];
      #ifdef VIRTUAL_SITES_DEBUG
      if (p==NULL){
         char *errtxt = runtime_error(128 + 3*TCL_INTEGER_SPACE);
         ERROR_SPRINTF(errtxt,"Particle does not exist in put_mol_force_on_parts! id=%i\n",topology[mol_id].part.e[i]);
         return NULL;
      }
      #endif
      if (ifParticleIsVirtual(p)) {
          return p;
       }
   }
#ifdef VIRTUAL_SITES_DEBUG
   char *errtxt = runtime_error(128 + 3*TCL_INTEGER_SPACE);
   ERROR_SPRINTF(errtxt,"No com found in get_mol_com_particleParticle does not exist in put_mol_force_on_parts! pnr=%i\n",calling_p->p.identity);
   return NULL;
#endif
   return calling_p;
}

double get_mol_dist(Particle *p1,Particle *p2){
   Particle *p1_com,*p2_com;
   double dist[3],dist2;
   p1_com=get_mol_com_particle(p1);
   p2_com=get_mol_com_particle(p2);
   #ifdef VIRTUAL_SITES_DEBUG
   if (p1_com==NULL){
      char *errtxt = runtime_error(128 + 3*TCL_INTEGER_SPACE);
      ERROR_SPRINTF(errtxt,"COM Particle not found for particle in get_mol_dist id=%i\n",p1->p.identity);
      dist[0]=dist[1]=dist[2]=0.0;
   }
   if (p2_com==NULL){
      char *errtxt = runtime_error(128 + 3*TCL_INTEGER_SPACE);
      ERROR_SPRINTF(errtxt,"COM Particle not found for particle in get_mol_dist id=%i\n",p2->p.identity);
      dist[0]=dist[1]=dist[2]=0.0;
   }
   #endif
   get_mi_vector(dist,p1_com->r.p, p2_com->r.p);
   dist2=SQR(dist[0])+SQR(dist[1])+SQR(dist[2]);
   return sqrt(dist2);
}

/** \name Statistic Functions */
/** \name Private Functions */
double calc_pressure_mol(int type1,int type2);
double calc_energy_kinetic_mol(int type);
void calc_force_between_mol(int mol_id1,int mol_id2,double force[3]);
#ifdef ELECTROSTATICS
void calc_total_dipolmoment_mol(int type,double total_dipole[4]);
void calc_absolute_dipolmoment_mol(int type,double total_dipole[2]);
void calc_dipole_of_molecule(int mol_id,double dipole[4]);
#endif

Particle *get_mol_com_particle_from_molid_cfg(int mol_id);
void get_mol_dist_vector_from_molid_cfg(int mol_id1,int mol_id2,double dist[3]);

int parse_and_print_pressure_mol(Tcl_Interp *interp,int argc, char **argv)
{
   char buffer[TCL_DOUBLE_SPACE];
   int type1, type2;
   double psum;
   #ifdef ELECTROSTATICS
   #ifndef INTER_RF
   Tcl_ResetResult(interp);
   Tcl_AppendResult(interp, "parse_and_print_pressure_mol is only possible with INTER_RF ", (char *)NULL);
   return (TCL_ERROR);
   #endif
   #endif
   updatePartCfg(WITHOUT_BONDS);
   if (!sortPartCfg()) {
      char *errtxt = runtime_error(128);
      ERROR_SPRINTF(errtxt, "{059 parse_and_print_pressure_mol: could not sort particle config, particle ids not consecutive?} ");
      return TCL_ERROR;
   }
   if (argc < 2) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "usage: analyze pressure_mol <type1> <type2>", (char *)NULL);
      return (TCL_ERROR);
   }

   if (!ARG0_IS_I(type1)) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "usage: analyze pressure_mol <type1> <type2>", (char *)NULL);
      return (TCL_ERROR);
   }
   if (!ARG1_IS_I(type2)) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "usage: analyze pressure_mol <type1> <type2>", (char *)NULL);
      return (TCL_ERROR);
   }
   argc-=2; argv+=2;

   if (n_molecules==0) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "No molecules defined !", (char *)NULL);
      return (TCL_ERROR);
   }
   psum=calc_pressure_mol(type1,type2);
   //sprintf(buffer,"%i",type1);
   //Tcl_AppendResult(interp,"{ analyze pressure_mol ",buffer," ",(char *)NULL);   
   //sprintf(buffer,"%i",type2);
   //Tcl_AppendResult(interp,buffer," ",(char *)NULL);   
   sprintf(buffer,"%e",psum);
   Tcl_AppendResult(interp, buffer,(char *)NULL);
   return TCL_OK;
}

int parse_and_print_energy_kinetic_mol(Tcl_Interp *interp,int argc, char **argv)
{
   char buffer[TCL_DOUBLE_SPACE];
   int type;
   double Ekin;
   updatePartCfg(WITHOUT_BONDS);
   if (!sortPartCfg()) {
      char *errtxt = runtime_error(128);
      ERROR_SPRINTF(errtxt, "{059 parse_and_print_energy_kinetic_mol: could not sort particle config, particle ids not consecutive?} ");
      return TCL_ERROR;
   }
   if (argc < 1) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "usage: analyze energy_kinetic <type>", (char *)NULL);
      return (TCL_ERROR);
   }

   if (!ARG0_IS_I(type)) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "usage: analyze energy_kinetic <type>", (char *)NULL);
      return (TCL_ERROR);
   }
   argc-=1; argv+=1;

   if (n_molecules==0) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "No molecules defined !", (char *)NULL);
      return (TCL_ERROR);
   }
   Ekin=calc_energy_kinetic_mol(type);
   if (Ekin < 0.0) {
     Tcl_ResetResult(interp);
      sprintf(buffer,"%i",-(int)Ekin);
      Tcl_AppendResult(interp,"Could not fetch com in calc_energy_kinetic_mol! From mol_id",buffer, (char *)NULL);
      return (TCL_ERROR);
   }
   //sprintf(buffer,"%i",type);
   //Tcl_AppendResult(interp,"{ analyze pressure_mol ",buffer," ",(char *)NULL);   
   sprintf(buffer,"%e",Ekin);
   Tcl_AppendResult(interp, buffer,(char *)NULL);
   return TCL_OK;
}

double calc_pressure_mol(int type1,int type2){
  double force[3], com_dist[3],psum=0;
  int i,j,k,l,start;
  for (i=0;i<n_molecules;i++){
    if (topology[i].type == type1){
      if (type1==type2){
	start=i+1;
      } else {
	start=0;
      }
      for (j=start;j<n_molecules;j++){
	for(l=0;l<3;l++)
	  force[l]=0;
	if (topology[j].type == type2){
	  get_mol_dist_vector_from_molid_cfg(i,j,com_dist);
	  calc_force_between_mol(i,j,force);
	  
	  for (k=0;k<3;k++){
	    psum+=force[k]*com_dist[k];
	  }
	  
	}
      }
    }
  }
  psum/=3.0;
  return psum;
}

int parse_and_print_dipole_mol(Tcl_Interp *interp,int argc, char **argv)
{
#ifndef ELECTROSTATICS
   Tcl_ResetResult(interp);
   Tcl_AppendResult(interp, "calc_dipole_mol is not possible without ELECTROSTATICS", (char *)NULL);
   return (TCL_ERROR);
#else
   int k,type;
   char buffer[TCL_DOUBLE_SPACE];
   double dipole[4];
   updatePartCfg(WITHOUT_BONDS);
   if (!sortPartCfg()) {
      char *errtxt = runtime_error(128);
      ERROR_SPRINTF(errtxt, "{059 parse_and_print_dipole: could not sort particle config, particle ids not consecutive?} ");
      return TCL_ERROR;
   }
   if (n_molecules==0) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "No molecules defined !", (char *)NULL);
      return (TCL_ERROR);
   }
   if (argc < 2) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "usage: analyze parse_and_print_dipole_mol <type>", (char *)NULL);
      return (TCL_ERROR);
   }

   if (!ARG1_IS_I(type)) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "usage: analyze parse_and_print_dipole_mol <type>", (char *)NULL);
      return (TCL_ERROR);
   }
   if (ARG0_IS_S("total")){
      calc_total_dipolmoment_mol(type,dipole);
      sprintf(buffer,"%i ",type);
      Tcl_AppendResult(interp,"{ dipolemoment_mol total ",buffer,(char *)NULL);
      for (k=0;k<3;k++)
      {
            sprintf(buffer,"%e ",dipole[k]);
            Tcl_AppendResult(interp, buffer,(char *)NULL);
      }
      sprintf(buffer,"%e",dipole[3]);
      Tcl_AppendResult(interp,buffer,"}",(char *)NULL);
   }
   else if (ARG0_IS_S("absolute")){
      calc_absolute_dipolmoment_mol(type,dipole);
      sprintf(buffer,"%i ",type);
      Tcl_AppendResult(interp,"{ dipolemoment_mol absolute ",buffer,(char *)NULL);
      sprintf(buffer,"%e ",dipole[0]);
      Tcl_AppendResult(interp, buffer,(char *)NULL);
      sprintf(buffer,"%e",dipole[1]);
      Tcl_AppendResult(interp,buffer,"}",(char *)NULL);
   }
   else
   {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "Feature not implemented", (char *)NULL);
      return (TCL_ERROR);
   }
   argc-=2; argv+=2;
   return TCL_OK;
#endif
}

void calc_force_between_mol(int mol_id1,int mol_id2,double force[3]){
  int i,j;
  Particle *p1,*p2;
  double vec12[3],dist2,dist;
#ifdef ADRESS
  int l;
  double weight;
  double temp_force[3]={0,0,0};
#endif
  force[0]=force[1]=force[2]=0.0;
  for (i=0;i<topology[mol_id1].part.n;i++){
    p1=&partCfg[topology[mol_id1].part.e[i]];
    for (j=0;j<topology[mol_id2].part.n;j++){
      p2=&partCfg[topology[mol_id2].part.e[j]];
      
      get_mi_vector(vec12,p1->r.p, p2->r.p);
      dist2=sqrlen(vec12);
      dist=sqrt(dist2);
#ifdef ADRESS
      for(l=0;l<3;l++)
	temp_force[l]=0;
#ifdef EXCLUSIONS
      if(do_nonbonded(p1,p2))
#endif
	{
	  calc_non_bonded_pair_force_from_partcfg_simple(p1,p2,vec12,dist,dist2,temp_force);
	  weight = adress_non_bonded_force_weight(p1,p2);
	  for(l=0;l<3;l++)
	    force[l] += weight*temp_force[l];
	}
#else
#ifdef EXCLUSIONS
      if(do_nonbonded(p1,p2))
#endif
	calc_non_bonded_pair_force_from_partcfg_simple(p1,p2,vec12,dist,dist2,force);
#endif
      
    }
  }
  
}

double calc_energy_kinetic_mol(int type){
   double E_kin=0;
   int i;
   Particle *p_com;
   for (i=0;i<n_molecules;i++){
      if (topology[i].type == type){
         p_com=get_mol_com_particle_from_molid_cfg(i);
#ifdef VIRTUAL_SITES_DEBUG
         if (p_com==NULL){
            return -(i);
         }
         if (!ifParticleIsVirtual(p_com)){
            return -(i);
         }
#endif
         E_kin+=PMASS(*p_com)*sqrlen(p_com->m.v);
      }
   }
   E_kin*=0.5/time_step/time_step;
   return E_kin;
}

#ifdef ELECTROSTATICS
void calc_absolute_dipolmoment_mol(int type,double average_dipole[2]){
   int i,j,count=0;
   double dipole[4],tmp;
   average_dipole[0]=average_dipole[1]=0.0;
   for (i=0;i<n_molecules;i++){
      if (topology[i].type == type){
         count++;
         calc_dipole_of_molecule(i,dipole);
         tmp=0.0;
         for (j=0;j<3;j++){
            tmp+=dipole[j]*dipole[j];
         }
         average_dipole[0]+=tmp;
         average_dipole[1]+=dipole[3];
      }
   }
   average_dipole[0]/=count;
   average_dipole[1]/=count;
}

void calc_total_dipolmoment_mol(int type,double total_dipole[4]){
   int i,j;
   double dipole[4];
   total_dipole[0]=total_dipole[1]=total_dipole[2]=total_dipole[3]=0.0;
   for (i=0;i<n_molecules;i++){
      if (topology[i].type == type){
         calc_dipole_of_molecule(i,dipole);
         for (j=0;j<4;j++){
            total_dipole[j]+=dipole[j];
         }
      }
   }
}

void calc_dipole_of_molecule(int mol_id,double dipole[4]){
   int j,k;
   Particle *p,*p_first=NULL;
   double vec12[3];
   dipole[0]=dipole[1]=dipole[2]=dipole[3]=0.0;
   for (j=0;j<topology[mol_id].part.n;j++){
      p=&partCfg[topology[mol_id].part.e[j]];
      if (ifParticleIsVirtual(p)) continue;
      if (p_first==NULL){
         p_first=p;
      }
      else
      {
         get_mi_vector(vec12,p->r.p, p_first->r.p);
         for (k=0;k<3;k++){
            dipole[k] += p->p.q*vec12[k];
         }
      }
      dipole[3]+=p->p.q;
   }
}
#endif

Particle *get_mol_com_particle_from_molid_cfg(int mol_id){
   int i;
   Particle *p;
   for (i=0;i<topology[mol_id].part.n;i++){
       p=&partCfg[topology[mol_id].part.e[i]];
       if (ifParticleIsVirtual(p)){
          return p;
       }
   }
#ifdef VIRTUAL_SITES_DEBUG
   fprintf(stderr,"No com found in get_mol_com_particle_from_molid_cfg ! mol_id=%i\n",mol_id);
#endif
   return NULL;
}

double get_mol_dist_partcfg(Particle *p1,Particle *p2){
   double dist[3],dist2;
   int mol_id1,mol_id2;
   mol_id1=p1->p.mol_id;
   mol_id2=p2->p.mol_id;
   get_mol_dist_vector_from_molid_cfg(mol_id1,mol_id2,dist);
   dist2=SQR(dist[0])+SQR(dist[1])+SQR(dist[2]);
   return sqrt(dist2);
}

void get_mol_dist_vector_from_molid_cfg(int mol_id1,int mol_id2,double dist[3]){
   Particle *p1_com,*p2_com;
   p1_com=get_mol_com_particle_from_molid_cfg(mol_id1);
   p2_com=get_mol_com_particle_from_molid_cfg(mol_id2);
   #ifdef VIRTUAL_SITES_DEBUG
   if(p1_com==NULL){
      fprintf(stderr,"No com found in get_mol_dist_vector_from_molid_cfg for mol id=%i\n",mol_id1);
      dist[0]=dist[1]=dist[2]=0.0;
      return;
   }
   if(p2_com==NULL){
      fprintf(stderr,"No com found in get_mol_dist_vector_from_molid_cfg for mol id=%i\n",mol_id2);
      dist[0]=dist[1]=dist[2]=0.0;
      return;
   }
   #endif
   get_mi_vector(dist,p1_com->r.p, p2_com->r.p);
}

int parse_and_check_mol_pos(Tcl_Interp *interp,int argc, char **argv){
   int j,count=0;
   double dist;
   char buffer[TCL_DOUBLE_SPACE];
   Particle p;
   updatePartCfg(WITHOUT_BONDS);
   for(j=0; j<n_total_particles; j++){
      if (!ifParticleIsVirtual(&partCfg[j])) continue;
      get_particle_data(j,&p);
      //dist=min_distance(partCfg[j].r.p,p.r.p);
      unfold_position(p.r.p,p.l.i);
      dist=distance(partCfg[j].r.p,p.r.p);
      if (dist > 0.01){
         if (count==0) Tcl_AppendResult(interp,"BEGIN Particle Missmatch: \n", (char *)NULL);
         sprintf(buffer,"%i",j);
         Tcl_AppendResult(interp,"Particle ",buffer, (char *)NULL);
         Tcl_PrintDouble(interp,partCfg[j].r.p[0] , buffer);
         Tcl_AppendResult(interp," partCfg x ",buffer, (char *)NULL);
         Tcl_PrintDouble(interp,partCfg[j].r.p[1] , buffer);
         Tcl_AppendResult(interp," y ",buffer, (char *)NULL);
         Tcl_PrintDouble(interp,partCfg[j].r.p[2] , buffer);
         Tcl_AppendResult(interp," z ",buffer, (char *)NULL);
         Tcl_PrintDouble(interp,p.r.p[0] , buffer);
         Tcl_AppendResult(interp," my_partCfg x ",buffer, (char *)NULL);
         Tcl_PrintDouble(interp,p.r.p[1] , buffer);
         Tcl_AppendResult(interp," y ",buffer, (char *)NULL);
         Tcl_PrintDouble(interp,p.r.p[2] , buffer);
         Tcl_AppendResult(interp," z ",buffer, (char *)NULL);
         Tcl_PrintDouble(interp, dist, buffer);
         Tcl_AppendResult(interp," dist ",buffer,"\n", (char *)NULL);
         count++;
      }
   }
   if (count!=0){
      Tcl_AppendResult(interp,"END Particle Missmatch\n", (char *)NULL);
      return(TCL_ERROR);
   }
   return(TCL_OK);
}

#endif

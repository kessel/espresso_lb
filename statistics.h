#ifndef STATISTICS_H
#define STATISTICS_H
/** \file statistics.h
    This file contains the code for simply statistics on the data.

    <b>Responsible:</b>
    <a href="mailto:mann@mpip-mainz.mpg.de">BAM</a>

*/

#include <tcl.h>
#include "particle_data.h"
#include "utils.h"

/** \name Data Types */
/************************************************************/
/*@{*/

typedef struct {
  /** Status flag for energy calculation. 0 re initialize energy
      struct, 1 every thing is fine, calculation can start. */
  int init_status;

  /** Array for energies on each node. */
  DoubleList node;
  /** Array for energies summed over all nodes. */
  DoubleList sum;

  /** number of energies. */
  int n;
  /** number of energies befor specific interaction energies. */
  int n_pre;
  /** number of energies for bonded interactions. */
  int n_bonded;
  /** number of energies for non bonded interactions. */
  int n_non_bonded;
  /** number of energies for coulomb interaction. */
  int n_coulomb;

  /** analyze specified energy. */
  int ana_num;
} Energy_stat;

/*@}*/

/** \name Exported Variables */
/************************************************************/
/*@{*/
extern Energy_stat energy;
/*@}*/

/** \name Exported Functions */
/************************************************************/
/*@{*/

/** Implements the Tcl command 'analyze <what> [<structure info>] [...]' for basic analysis.
    Possible arguments for <what> are:
    <ul>
    <li> 'analyze mindist' <BR>
         returns the minimal distance of two particles (needs no structure info).
    <li> 'analyze nbhood <part_id> <r_catch>' <BR>
         returns all particles within a given radius <r_catch> around the position of particle <part_id>.
    <li> 'analyze distto { <part_id> | <posx> <posy> <posz> }' <BR>
         returns the minimal distance of all particles to coordinates (<posx>, <posy>, <posz>) or to the position of particle <part_id>.
    <li> 'analyze energy [interaction]' <BR>
         returns the energies of the system. output is blockfile format: <BR>
	 { energy <value> } { kinetic <value> } { interaction <value> } ... <BR>
	 if you specify an interaction, e.g. fene <type_num> or lj <type1> <type2> or coulomb or kinetic
	 it returns just that energy.
    <li> 'analyze set <structure info>' <BR>
         defines the structure. The second argument defines the topology to set, i. e. chain at the moment.<BR>
	 Possible values for <structure info> are
	 <ul>
	 <li> 'analyze set chains [<chain_start> <n_chains> <chain_length>]'
              A set of equal-length chains. If no parameters are given, the ones currently stored are returned.
	 </ul>
    </ul>
    All tasks below need the particles to be stored consecutively starting with identity 0, and the structure info to be either set by
    'analyze set chains <chain_start> <n_chains> <chain_length>' above or to be provided upon calling (which (re-)sets 
    the structure info permanently, i.e. is only required once):
    <ul>
    <li> 'analyze re [<chain_start> <n_chains> <chain_length>]' <BR>
         returns the root of the quadratic end-to-end-distance averaged over all polymers (requires chain structure to be set or provided).
    <li> 'analyze rg [<chain_start> <n_chains> <chain_length>]' <BR>
         returns the radius of gyration averaged over all chains (requires chain structure to be set or provided).
    <li> 'analyze rh [<chain_start> <n_chains> <chain_length>]' <BR>
         returns the hydrodynamic radius (requires chain structure to be set or provided).
    <li> 'analyze g123 [[-init] <chain_start> <n_chains> <chain_length>]' <BR>
         returns the mean-square displacement g1(t) of a monomer,
                 the mean-square displacement g2(t) in the center of gravity of the chain itself, and
                 the motion of the center of mass g3(t)
	 as a tcl-list {g1(t) g2(t) g3(t)} (requires chain structure to be set or provided). <BR>
	 If before the structure info you give '-init', the current configuration is stored as reference config.
    </ul>
*/
int analyze(ClientData data, Tcl_Interp *interp, int argc, char **argv);

void init_energies();

void calc_energy();

/** the minimal distance of two particles.
    @return the minimal distance of two particles */
double mindist();

/** returns all particles within a given radius r_catch around a position.
    @param posx x-coordinate of point
    @param posy y-coordinate of point
    @param posz z-coordinate of point
    @param r_catch the radius around the position
    @param il the list where to store the particles indices */
void nbhood(double posx, double posy, double posz, double r_catch, IntList *il);

/** minimal distance to point.
    @param posx x-coordinate of point
    @param posy y-coordinate of point
    @param posz z-coordinate of point
    @param pid  if a valid particle id, this particle is omitted from minimazation
                (this is a good idea if the posx, posy, posz is the position of a particle).
    @return the minimal distance of a particle to coordinates (<posx>, <posy>, <posz>). */
double distto(double posx, double posy, double posz, int pid);

/** calculate the end-to-end-distance. chain information \ref chain_start etc. must be set!
    @return the end-to-end-distance */
double calc_re();

/** calculate the radius of gyration. chain information \ref chain_start etc. must be set!
    @return the radius of gyration */
double calc_rg();

/** calculate the hydrodynamic radius. chain information \ref chain_start etc. must be set!
    @return the hydrodynamic radius */
double calc_rh();

/** calculate g123. chain information \ref chain_start etc. must be set!
    @param g1 contains g1
    @param g2 contains g2
    @param g3 contains g3
*/
void calc_g123(double *g1, double *g2, double *g3);

/** set the start configuration for g123.
    chain information \ref chain_start etc. must be set!
*/
void init_g123();

/*@}*/

#endif

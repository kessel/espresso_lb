// This file is part of the ESPResSo distribution (http://www.espresso.mpg.de).
// It is therefore subject to the ESPResSo license agreement which you accepted upon receiving the distribution
// and by which you are legally bound while utilizing this file in any form or way.
// There is NO WARRANTY, not even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// You should have received a copy of that license along with this program;
// if not, refer to http://www.espresso.mpg.de/license.html where its current version can be found, or
// write to Max-Planck-Institute for Polymer Research, Theory Group, PO Box 3148, 55021 Mainz, Germany.
// Copyright (c) 2002-2004; all rights reserved unless otherwise stated.

/** \file rattle.h    RATTLE Algorithm (Rattle: A "Velocity" Version of the Shake
 *                    Algorithm for Molecular Dynamics Calculations, H.C Andersen,
 *                    J Comp Phys, 52, 24-34, 1983)
 *
 *  <b>Responsible:</b>
 *  <a href="mailto:arijitmaitra@uni-muenster.de">Arijit</a>
 *
 *  For more information see \ref rattle.c "rattle.c".
*/
#include <tcl.h>
#include "global.h"
#include "particle_data.h"

/** Transfers the current particle positions from r.p[3] to r.p_pold[3]
    of the \ref Particle structure. Invoked from \ref correct_pos_shake() */
void save_old_pos();

/** Calculates the corrections required for each of the particle coordinates
    according to the RATTLE algorithm. Invoked from \ref correct_pos_shake()*/
void compute_pos_corr_vec();

/** Positinal Corrections are added to the current particle positions. Invoked from \ref correct_pos_shake() */
void app_correction_check_VL_rebuild();

/** Tolerance for positional corrections are checked, which is a criteria
    for terminating the SHAKE/RATTLE iterations. Invoked from \ref correct_pos_shake() */
int check_tol_pos();

/** Propagate velocity and position while using SHAKE algorithm for bond constraint.*/
void correct_pos_shake();

/** Transfers temporarily the current forces from f.f[3] of the \ref Particle
    structure to r.p_old[3] location and also intialize velocity correction
    vector. Invoked from \ref correct_vel_shake()*/
void transfer_force_init_vel();

/** Calculates corrections in current particle velocities according to RATTLE
    algorithm. Invoked from \ref correct_vel_shake()*/
void compute_vel_corr_vec();

/** Velocity corrections are added to the current particle velocities. Invoked from
    \ref correct_vel_shake()*/
void apply_vel_corr();

/** Check if tolerance in velocity is satisfied, which is a criterium for terminating
    velocity correctional iterations. Invoked from \ref correct_vel_shake()  */
int check_tol_vel();

/*Invoked from \ref correct_vel_shake()*/
void revert_force();

/** Correction of current velocities using RATTLE algorithm*/
void correct_vel_shake();

void print_bond_len();


MDINLINE void vector_subt(double res[3], double a[3], double b[3])
{
   int i;
   for (i=0;i<3;i++)
        res[i]=a[i]-b[i];
}


// This file is part of the ESPResSo distribution (http://www.espresso.mpg.de).
// It is therefore subject to the ESPResSo license agreement which you accepted upon receiving the distribution
// and by which you are legally bound while utilizing this file in any form or way.
// There is NO WARRANTY, not even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// You should have received a copy of that license along with this program;
// if not, refer to http://www.espresso.mpg.de/license.html where its current version can be found, or
// write to Max-Planck-Institute for Polymer Research, Theory Group, PO Box 3148, 55021 Mainz, Germany.
// Copyright (c) 2002-2009; all rights reserved unless otherwise stated.
#ifndef CONFIG_H
#define CONFIG_H

/** \file config.h

    This file contains the defaults for Espresso. To modify them, add
    an appropriate line in myconfig.h. To find a list of features that
    can be compiled into Espresso, refer to myconfig-sample.h or to
    \ref tcl_features "the documentation of the features".
 */

/* Include the defines created by configure. */
#include <acconfig.h>

/************************************************/
/** \name Default Parameter Settings            */
/************************************************/
/*@{*/

/** CELLS: Default value for the maximal number of cells per node. */
#define CELLS_MAX_NUM_CELLS 32768

/** P3M: Default for number of interpolation points of the charge
    assignment function. */
#define P3M_N_INTERPOL 32768

/** P3M: Default for boundary condition: Epsilon of the surrounding
    medium. */
#define P3M_EPSILON 0.0

/** P3M: Default for boundary condition in magnetic calculations */
#define P3M_EPSILON_MAGNETIC 1.0

/** P3M: Default for offset of first mesh point from the origin (left
    down corner of the simulation box. */
#define P3M_MESHOFF 0.5

/** P3M: Default for the number of Brillouin zones taken into account
    in the calculation of the optimal influence function (aliasing
    sums). */
#define P3M_BRILLOUIN 1

/** P3M: Maximal mesh size that will be checked. The current setting
         limits the memory consumption to below 1GB, which is probably
	 reasonable for a while. */
#define P3M_MAX_MESH 128

/** Whether to use the approximation of Abramowitz/Stegun
    AS_erfc_part() for \f$\exp(d^2) erfc(d)\f$, or the C function erfc
    in P3M and Ewald summation. */
#define USE_ERFC_APPROXIMATION 1

/** Precision for capture of round off errors. */
#define ROUND_ERROR_PREC 1.0e-14

/** Tiny angle cutoff for sinus calculations */
#define TINY_SIN_VALUE 1e-10
/** Tiny angle cutoff for cosine calculations */
#define TINY_COS_VALUE 0.9999999999
/** Tiny length cutoff */
#define TINY_LENGTH_VALUE 0.0001


/** maximal number of iterations in the RATTLE algorithm before it bails out. */
#define SHAKE_MAX_ITERATIONS 1000

/*@}*/


#ifdef MYCONFIG_H
#include MYCONFIG_H
#endif

//inter_rf needs ELECTROSTATICS
#ifdef INTER_RF
#define ELECTROSTATICS
#endif

#ifdef GAY_BERNE
#define ROTATION
#endif

/* activate P3M only with FFTW */
#if defined(ELECTROSTATICS) && defined(FFTW)
#define ELP3M
#endif


/* activate dipolar P3M only with FFTW */
#if defined(MAGNETOSTATICS) && defined(FFTW)
#define ELP3M
#define DIPOLES
#endif


/* MAGNETOSTATICS implies the use of DIPOLES */
#if defined(MAGNETOSTATICS)
#define DIPOLES
#endif


/* LB_ELECTROHYDRODYNAMICS needs LB, obviously... */
#if defined(LB_ELECTROHYDRODYNAMICS) && !defined(LB)
#define LB
#endif

/* LB_BOUNDARIES need constraints, obviously... */
#if defined(LB_BOUNDARIES) && !defined(CONSTRAINTS)
#define CONSTRAINTS 
#endif

/* Lattice Boltzmann needs lattice structures and temporary particle data */
#ifdef LB
#define USE_TEMPORARY
#define LATTICE
//#define ALTERNATIVE_INTEGRATOR
#endif

//adress needs mol_cut
#ifdef ADRESS
#define MOL_CUT
#endif

//mol_cut needs virtual sites
#ifdef MOL_CUT
#define VIRTUAL_SITES
#endif

#if defined(DPD_MASS_RED) || defined(DPD_MASS_LIN)
#define DPD_MASS
#endif

/*DPD with mass needs MASS and DPD */
#ifdef DPD_MASS
#define MASS
#endif

/*Transversal DPD -> needs normal DPD*/
#ifdef TRANS_DPD
#define DPD
#endif

/* If any bond angle potential is activated, actiavte the whole bond angle code */
#if defined(BOND_ANGLE_HARMONIC) || defined(BOND_ANGLE_COSINE) || defined(BOND_ANGLE_COSSQUARE)
#define BOND_ANGLE
#endif

/* If any bond angledist potential is activated, activate the whole bond angle code and constraints */
#if defined(BOND_ANGLEDIST_HARMONIC)
#define BOND_ANGLEDIST
#define CONSTRAINTS
#endif

#if defined(BOND_ENDANGLEDIST_HARMONIC)
#define BOND_ENDANGLEDIST
#define CONSTRAINTS
#endif

/********************************************/
/* \name exported functions of config.c     */
/********************************************/
/*@{*/
#include <tcl.h>

/** callback for version status. */
int version_callback(Tcl_Interp *interp);

/** callback for compilation status. */
int compilation_callback(Tcl_Interp *interp);
/*@}*/

#endif

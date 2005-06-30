
package require mmsg 0.1.0
package provide std_analysis 0.1.0

namespace eval ::std_analysis {
    variable area_lipid
    variable iotype 
    variable all_particles
    variable rawmodes
 
    variable l_orients_start
    variable suffix

    variable known_flags " possible flags are: \n cluster_calc \n pik1_calc \n pressure_calc \n box_len_calc \n fluctuation_calc \n energy_calc \n stray_lipids_calc \n orient_order_calc \n flipflop_calc \n distance_calc \n"


    #File Streams
    variable f_tvspik1
    variable f_tvsp
    variable f_tvsbl
    variable f_tvsflip
    variable f_tvsoop
    variable f_tvsstray
    variable f_tvsen
    variable f_tvsclust

    variable f_tvsdist

    # Averaging
    variable av_pow
    variable av_pow_i 0

    variable av_sizehisto 0
    variable av_sizehisto_i 0

    variable av_clust { 0 0 0 0 0 0 0 0 0 0 0 }
    variable av_clust_i 0

    variable av_pik1 { 0 0 0 0 0 0 0 0 0 }
    variable av_pik1_i 0

    variable av_pressure { 0 0 0 0 0 0 }
    variable av_pressure_i 0

    variable av_boxl { 0 0 0 }
    variable av_boxl_i 0

    variable av_flip 0.0
    variable av_flip_i 0

    variable av_oop 0.0
    variable av_oop_i 0
    
    variable av_stray 0
    variable av_stray_i 0


    variable av_dist  0 
    variable av_dist_i 0

    variable av_components_en 0
    variable av_total_en 0
    variable av_kin_en 0
    variable av_fene_en 0
    variable av_harm_en 0
    variable av_nb_en 0
    variable av_en_i 0

    variable topology

    variable switches

    variable this [namespace current]

    namespace export do_analysis
    namespace export setup_analysis
    namespace export print_averages
    namespace export flush_streams

}

source [file join [file dirname [info script]] flipflop.tcl]
source [file join [file dirname [info script]] boxl.tcl]
source [file join [file dirname [info script]] clusters.tcl]
source [file join [file dirname [info script]] energy.tcl]
source [file join [file dirname [info script]] pressure.tcl]
source [file join [file dirname [info script]] pik1.tcl]
source [file join [file dirname [info script]] oop.tcl]
source [file join [file dirname [info script]] fluctuations.tcl]
source [file join [file dirname [info script]] stray.tcl]


source [file join [file dirname [info script]] distance.tcl]

# ::std_analysis::flush_streams --
#
# Flush all of the output stream associated with std_analysis
#
proc ::std_analysis::flush_streams { } {
    variable known_flags
    variable switches
    variable this
    variable f_tvspik1
    variable f_tvsp
    variable f_tvsbl
    variable f_tvsflip
    variable f_tvsoop
    variable f_tvsen
    variable f_tvsclust
    variable f_tvsstray

    variable f_tvsdist


    for { set i 0 } { $i < [llength $switches ] } { incr i } {
	switch [lindex $switches $i 0] {
	    "cluster_calc" {
		flush $f_tvsclust
	    }
	    "pik1_calc" {
		flush $f_tvspik1
	    }
	    "pressure_calc" {
		flush $f_tvsp
	    }
	    "box_len_calc" {
		flush $f_tvsbl
	    }
	    "fluctuation_calc" {
	    }
	    "flipflop_calc" {
		flush $f_tvsflip
	    }
	    "orient_order_calc" {
		flush $f_tvsoop
	    }
	    "energy_calc" {
		flush $f_tvsen
	    }
	    "stray_lipids_calc" {
		flush $f_tvsstray
	    }
	    "distance_calc" {
		flush $f_tvsdist
	    }
	    "default" {
		mmsg::warn $this "unknown analysis flag [lindex $switches $i 0] $known_flags" 
	    }
	}	    
    }
}

# ::std_analysis::print_averages --
#
# Calculate averages for all analyzed quantities and put them to the
# appropriate file streams
#
proc ::std_analysis::print_averages { } {
    variable this
    mmsg::debug $this "printing averages"
    variable switches
    variable known_flags
    variable av_pik1_i
    variable av_pik1
    variable f_tvspik1

    variable av_clust
    variable av_clust_i
    variable f_tvsclust

    variable av_pressure
    variable av_pressure_i
    variable f_tvsp

    variable av_boxl_i
    variable av_boxl
    variable f_tvsbl

    variable av_pow_i
    variable av_pow
    
    variable av_flip
    variable av_flip_i
    variable f_tvsflip
    
    variable av_oop
    variable av_oop_i
    variable f_tvsoop
    
    variable av_stray
    variable av_stray_i
    variable f_tvsstray


    variable av_dist_i
    variable av_dist
    variable f_tvsdist

    
    variable av_components_en
    variable av_total_en
    variable av_kin_en
    variable av_fene_en
    variable av_harm_en
    variable av_nb_en
    variable av_en_i
    
    
    variable f_tvsen
    
    variable av_sizehisto
    variable av_sizehisto_i
    variable outputdir
    
    set time [setmd time]
    
    mmsg::debug $this "calculating [llength $switches ] different quantities"
    for { set i 0 } { $i < [llength $switches ] } { incr i } {
	#	    puts [lindex $switches $i 0]
	#	    flush stdout
	switch [lindex $switches $i 0] {
	    "cluster_calc" {
		if { [lindex $switches $i 2] && $av_clust_i > 0 } {
		    puts -nonewline $f_tvsclust "$time "
		    for { set v 0 } { $v < [llength $av_clust] } {incr v} {
			puts -nonewline $f_tvsclust "[expr [lindex $av_clust $v]/($av_clust_i*1.0)] "
		    }
		    puts $f_tvsclust " "
		    
		    set tident [expr int(floor($time))]
		    set tmpfile [open "$outputdir/sizehisto.[format %05d $tident]" w ]
		    for {set v 0 } { $v < [llength $av_sizehisto] } { incr v } {
			puts $tmpfile "[expr $v + 1] [expr ([lindex $av_sizehisto $v]/(1.0*$av_sizehisto_i))]"
		    }
		    close $tmpfile
		    
		} else {
		    mmsg::warn $this "can't print average clusters"
		    flush stdout
		}
	    }
	    "pik1_calc" {
		if { [lindex $switches $i 2] && $av_pik1_i > 0 } {
		    puts -nonewline $f_tvspik1 "$time "
		    for { set v 0 } { $v < [llength $av_pik1] } {incr v} {
			puts -nonewline $f_tvspik1 "[expr [lindex $av_pik1 $v]/($av_pik1_i*1.0)] "
			
		    }
		    puts $f_tvspik1 " "
		} else {
		    mmsg::warn $this "can't print average pik1"
		    flush stdout		       
		}
	    }
	    "pressure_calc" {
		if { [lindex $switches $i 2] && $av_pressure_i > 0 } {
		    puts -nonewline $f_tvsp "$time "
		    for { set v 0 } { $v < [llength $av_pressure] } {incr v} {
			puts -nonewline $f_tvsp "[expr [lindex $av_pressure $v]/($av_pressure_i*1.0)] "
			
		    }
		    puts $f_tvsp " "
		} else {
		    mmsg::warn $this "can't print average pressure"
		    flush stdout		       
		}
	    }
	    "box_len_calc" {
		if { [lindex $switches $i 2] && $av_boxl_i > 0 } {
		    set avblx [expr [lindex $av_boxl 0]/($av_boxl_i*1.0)]
		    set avbly [expr [lindex $av_boxl 1]/($av_boxl_i*1.0)]
		    set avblz [expr [lindex $av_boxl 2]/($av_boxl_i*1.0)]
		    puts $f_tvsbl "$time $avblx $avbly $avblz"
		    #			set av_boxl_i 0
		} else {
		    mmsg::warn $this "can't print average box length"
		    flush stdout
		}
		
	    }
	    "fluctuation_calc" {
		if { [lindex $switches $i 2] && $av_pow_i > 0 } {
		    # Do a power analysis on the intermediate results
		    power_analysis
		}
	    }
	    "flipflop_calc" {
		puts $f_tvsflip "$time [expr $av_flip/(1.0*$av_flip_i)]"
	    }
	    "orient_order_calc" {
		puts $f_tvsoop "$time [expr $av_oop/(1.0*$av_oop_i)]"
	    }
	    "stray_lipids_calc" {
		puts $f_tvsstray "$time [expr $av_stray/(1.0*$av_stray_i)]"
	    }
	    "energy_calc" {
		puts -nonewline $f_tvsen "$time [expr $av_total_en/(1.0*$av_en_i)] [expr $av_fene_en/(1.0*$av_en_i)] [expr $av_kin_en/(1.0*$av_en_i)] [expr $av_harm_en/(1.0*$av_en_i)] [expr $av_nb_en/(1.0*$av_en_i)]"
		foreach comp $av_components_en {
		    puts -nonewline $f_tvsen " [expr $comp/(1.0*$av_en_i)]"
		}
		puts $f_tvsen ""
	    }


	    "distance_calc" {
		if { [lindex $switches $i 2] && $av_dist_i > 0 } {
		    set avdistx [expr [lindex $av_dist 0]/($av_dist_i*1.0)]
		  
		    puts $f_tvsdist "$time $avdistx"
		    #			set av_boxl_i 0
		} else {
		    mmsg::warn $this "can't print average distance"
		    flush stdout
		}
		
	    }


	    "default" {
		mmsg::warn $this "unknown analysis flag [lindex $switches $i 0] $known_flags" 
	    }
	}
    }
    reset_averages
    #	puts "done"
    flush stdout
}


# ::std_analysis::reset_averages --
#
# Reset all of the average storage variables and counters to zero
#  
#  Note: Power analysis is not reset since it generally requires
#  averages over the entire simulation. Flip-flop is also not reset
#  since it should exponentially decay with time and is calculated
#  from the entire simulation run.
#
proc ::std_analysis::reset_averages { } {
    variable av_sizehisto
    variable av_sizehisto_i
    variable known_flags

    variable av_clust
    variable av_clust_i
    
    variable av_pik1_i
    variable av_pik1
    
    variable av_pressure
    variable av_pressure_i
    
    variable av_boxl_i
    variable av_boxl
    
    variable av_flip
    variable av_flip_i
    
    variable av_oop
    variable av_oop_i
    
    variable av_stray
    variable av_stray_i


    variable av_dist_i
    variable av_dist
    
    variable av_components_en
    variable av_total_en
    variable av_kin_en
    variable av_fene_en
    variable av_harm_en
    variable av_nb_en
    variable av_en_i
    
    set av_sizehisto 0
    set av_sizehisto_i 0
    
    
    set av_clust {0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 }
    set av_clust_i 0
    
    set av_boxl { 0.0 0.0 0.0 }
    set av_boxl_i 0
    
    set av_pik1 { 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 }
    set av_pik1_i 0
    
    set av_pressure {0.0 0.0 0.0 0.0 0.0 0.0 }
    set av_pressure_i 0
    
    set av_flip 0
    set av_flip_i 0
    
    set av_oop 0
    set av_oop_i 0
    
    set av_stray 0
    set av_stray_i 0


    set av_dist 0.0
    set av_dist_i 0

    
    for {set i 0 } {$i < [llength $av_components_en] } { incr i } {
	lset av_components_en $i 0.0
    }
    set av_total_en 0
    set av_kin_en 0
    set av_fene_en 0
    set av_harm_en 0
    set av_nb_en 0
    set av_en_i 0
    

    
}

# ::std_analysis::do_analysis --
#
# This is the routine that is typically called during the simulation
# after each integrate command.  It simply calls all of the
# appropriate analysis routines and stores the values in average
# storage variables
#
proc ::std_analysis::do_analysis { } {
    variable switches
    variable this
    variable known_flags

    for { set i 0 } { $i < [llength $switches ] } { incr i } {
	switch [lindex $switches $i 0] {
	    "cluster_calc" {
		analyze_clusters [lindex $switches $i 1]
	    }
	    "pik1_calc" {
		analyze_pik1 [lindex $switches $i 1]
	    }
	    "pressure_calc" {
		analyze_pressure [lindex $switches $i 1]
	    }
	    "quick_pressure_calc" {
		analyze_quick_pressure [lindex $switches $i 1]
	    }
	    "box_len_calc" {
		analyze_box_len [lindex $switches $i 1]
	    }
	    "fluctuation_calc" {
		analyze_fluctuations [lindex $switches $i 1]
	    }
	    "flipflop_calc" {
		analyze_flipflop [lindex $switches $i 1]
	    }
	    "orient_order_calc" {
		analyze_oop [lindex $switches $i 1]
	    }
	    "stray_lipids_calc" {
		analyze_stray [lindex $switches $i 1]
	    }
	    "energy_calc" {
		analyze_energy [lindex $switches $i 1]
	    }


	    "distance_calc" {
		analyze_distance [lindex $switches $i 1]
	    }


	    "default" {
		mmsg::warn $this "unknown analysis flag [lindex $switches $i 0] $known_flags" 
	    }
	}	    
    }
    mmsg::debug $this "done"
    flush stdout
}

# ::std_analysis::do_analysis --
#
# This routine should be called once and only once at the beginning of
# the simulation in order to setup all the appropriate variables that
# will later be used when do_analysis is called.
#
# Arguments:
#
#        switchesin: This argument should consist of a list of
#                    switches that determine which quantites should be
#                    analyzed and whether results should be printed to
#                    stdout during calculation. To see what this list
#                    looks like just look at the example
#                    parameters.tcl file 
#
#
#       mgrid:       Size of the grid used for calculation of a height grid.
#                    This is used for flip-flop fluctation and stray
#                    lipid routines.
#
#       outputdir:   Output directory
#
#       straycutoff: The Cutoff distance beyond which a lipid is
#                    considered to be a stray
#
#
#       a_lipid:     The area per lipid
#
#       suffix: The suffix to use for output files.  
#
#       iotype: This parameter set the iotype of the files that will
#                    be opened for writing output.  Set this to "a" if
#                    you want to append to old data or to "w" if you
#                    want to overwrite.
#
proc ::std_analysis::setup_analysis { switchesin topo args } {

    variable suffix
    variable known_flags
    variable iotype
    variable switches
    variable n_particles
    variable mgrid
    variable outputdir
    variable stray_cut_off
    variable all_particles
    variable f_tvspik1
    variable f_tvsp
    variable f_tvsbl
    variable f_tvsflip
    variable f_tvsoop
    variable f_tvsstray
    variable f_tvsen
    variable f_tvsclust
    variable av_pow
    variable l_orients_start
    variable topology
    variable area_lipid
    variable this


    variable f_tvsdist


    variable av_components_en

    mmsg::send $this "setting up analysis"

    set options {
	{mgrid.arg      8    "set the size of the grid for heightfunction calculations" }
	{straycutoff.arg      3    "stray distance from bilayer " }
	{outputdir.arg      "./"    "name of output directory " }
	{alipid.arg  1.29 "area per lipid" }
	{suffix.arg "tmp" "suffix to be used for outputfiles" }
	{iotype.arg "a" "the method with which to open existing analysis files"}
    }
    set usage "Usage: setup_analysis gridm:straycutoff:outputdir:alipid:suffix:iotype: "
    array set params [::cmdline::getoptions args $options $usage]


    # This checks for a common problem in the formatting of the $switchesin
    # and attempts for construct a list called switches that is
    # correct

    if { [llength [lindex [lindex $switchesin 0] 0 ] ] == 3 } {
	set switches ""
	for { set i 0 } { $i < [ llength [lindex $switchesin 0] ] } {incr i} {
	    lappend switches [lindex [lindex $switchesin 0] $i ]
	}
    } else {
	set switches $switchesin
    }

    # Calculate the total number of particles from topology
    set n_particles 0
    foreach mol $topo {
	set n_particles [expr $n_particles + [llength $mol] -1]
    }

    set mgrid $params(mgrid)
    set outputdir $params(outputdir)
    set stray_cut_off $params(straycutoff)
    set topology $topo
    set iotype $params(iotype)
    set suffix "_$params(suffix)"
    set area_lipid $params(alipid)

    for { set i 0 } { $i < [llength $switches ] } { incr i } {
	mmsg::debug $this "switch = [lindex $switches $i 0]"
	switch [lindex $switches $i 0] {
	    "cluster_calc" {
		mmsg::debug $this "opening $outputdir/time_vs_clust$suffix "		    

		if { [file exists "$outputdir/time_vs_clust$suffix"] } {
		    set newfile 0
		} else { 
		    set newfile 1
		}

		set f_tvsclust [open "$outputdir/time_vs_clust$suffix" $iotype]
		if { $newfile || $iotype == "w"} {
		    puts $f_tvsclust "\# cmax cmin c2sizeav c2sizestd nc2 csizeav csizestd nc clenav clenstd nc"
		}

	    }
	    "pik1_calc" {
		for { set j 0 } { $j < $n_particles } { incr j } {
		    lappend all_particles $j
		}
		mmsg::debug $this "opening $outputdir/time_vs_pik1$suffix "

		if { [file exists "$outputdir/time_vs_pik1$suffix"] } {
		    set newfile 0
		} else { 
		    set newfile 1
		}
		set f_tvspik1 [open "$outputdir/time_vs_pik1$suffix" $iotype]
		if { $newfile || $iotype == "w"} {
		    puts $f_tvspik1 "\# Components of the total pressure tensor in row major order"
		    puts $f_tvspik1 "\# pxx pxy pxz pyx pyy pyz pzx pzy pzz"
		}
	    }
	    "pressure_calc" {
		mmsg::debug $this "Opening $outputdir/time_vs_pressure$suffix "

		if { [file exists "$outputdir/time_vs_pressure$suffix"] } {
		    set newfile 0
		} else { 
		    set newfile 1
		}

		set f_tvsp [open "$outputdir/time_vs_pressure$suffix" $iotype]
		if { $newfile || $iotype == "w"} {
		    puts $f_tvsp "\# Components of the total pressure"
		    puts $f_tvsp "\# Time p_inst(NPT only) total ideal fene harmonic nonbonded "
		}
	    }
	    "box_len_calc" {
		mmsg::debug $this "opening $outputdir/time_vs_boxl$suffix "

		if { [file exists "$outputdir/time_vs_boxl$suffix"] } {
		    set newfile 0
		} else { 
		    set newfile 1
		}
		set f_tvsbl [open "$outputdir/time_vs_boxl$suffix" $iotype]
		if { $newfile || $iotype == "w"} {
		    puts $f_tvsbl "\# Time boxx boxy boxz"
		}

	    }
	    "fluctuation_calc" {
		for { set r 0 } { $r < [expr $mgrid*($mgrid)] } { incr r } {
		    lappend av_pow 0.0
		}
		set rawmodes "[analyze modes2d setgrid $mgrid $mgrid 0 setstray $stray_cut_off]"
	    }
	    "flipflop_calc" {
		mmsg::debug $this "opening $outputdir/time_vs_flip$suffix "

		if { [file exists "$outputdir/time_vs_flip$suffix"] } {
		    set newfile 0
		} else { 
		    set newfile 1
		}

		set f_tvsflip [open "$outputdir/time_vs_flip$suffix" $iotype]
		set f_tvsflip [open "$outputdir/time_vs_flip$suffix" $iotype]
		if { $newfile || $iotype == "w"} {
		    puts $f_tvsflip "\# Note: F(t) = N_up(t) + N_d(t)/N. See long membrane paper for details"
		    puts $f_tvsflip "\# Time flip_parameter"
		}
		set l_orients_start [ analyze get_lipid_orients setgrid $mgrid $mgrid 0 setstray $stray_cut_off ]
	    }
	    "orient_order_calc" {

		if { [file exists "$outputdir/time_vs_oop$suffix"] } {
		    set newfile 0
		} else { 
		    set newfile 1
		}
		mmsg::debug $this "opening $outputdir/time_vs_oop$suffix "
		set f_tvsoop [open "$outputdir/time_vs_oop$suffix" $iotype ]
		if { $newfile || $iotype == "w"} {
		    puts $f_tvsoop "\# S = 0.5*<3*(a_i.n)^2 -1>i"
		    puts $f_tvsoop "\# where a_i is the orientation of the ith lipid and n is the average bilayer normal"
		    puts $f_tvsoop "\# Time S"
		}
		

	    }
	    "stray_lipids_calc" {
		if { [file exists "$outputdir/time_vs_stray$suffix"] } {
		    set newfile 0
		} else { 
		    set newfile 1
		}
		mmsg::debug $this "opening $outputdir/time_vs_stray$suffix "
		set f_tvsstray [open "$outputdir/time_vs_stray$suffix" $iotype ]
		if { $newfile || $iotype == "w"} {
		    puts $f_tvsstray "\# The number of stray lipids vs time"
		    puts $f_tvsstray "\# Time num_strays"
		}
		set tmp [ analyze get_lipid_orients setgrid $mgrid $mgrid 0 setstray $stray_cut_off ]
	    }
	    "energy_calc" {

		if { [file exists "$outputdir/time_vs_energy$suffix"] } {
		    set newfile 0
		} else { 
		    set newfile 1
		}
		mmsg::debug $this "opening $outputdir/time_vs_energy$suffix "
		set f_tvsen [open "$outputdir/time_vs_energy$suffix" $iotype ]
		# First work out the names of components
		set raw [analyze energy]

		if { $newfile || $iotype == "w"} {
		    puts $f_tvsen "\# Components of the energy "
		    puts -nonewline $f_tvsen "\# Time total_en fene_en kinetic_en harmonic_en nonbonded_en"
		}
		unset av_components_en
		for { set k 0 } { $k < [llength $raw ] } { incr k } {
		    set tmp [lindex $raw $k]
		    set ntmp [llength $tmp]	
		    if { [ regexp "nonbonded" $tmp ] } {
			puts -nonewline $f_tvsen " [lrange $tmp 0 end-1]"
			lappend av_components_en 0.0
		    }
		}
		puts $f_tvsen ""

	    }


	     "distance_calc" {
		mmsg::debug $this "opening $outputdir/time_vs_distance$suffix "

		if { [file exists "$outputdir/time_vs_distance$suffix"] } {
		    set newfile 0
		} else { 
		    set newfile 1
		}
		set f_tvsdist [open "$outputdir/time_vs_distance$suffix" $iotype]
		if { $newfile || $iotype == "w"} {
		    puts $f_tvsdist "\# Time Distance"
		}

	    }


	    "default" {
		mmsg::warn $this "unknown analysis flag [lindex $switches $i 0] $known_flags" 
	    }
	}
	
    }
    mmsg::debug $this "done"
    flush stdout
}

# ::std_analysis::finish_analysis --
#
# Cleanup by unsetting variables closing files etc.  If this is called
# then it should be OK to call setup_analysis to create an entirely
# new analysis setup.
#
proc ::std_analysis::finish_analysis {  } { 
    mmsg::send $this "finishing analysis"
    variable all_particles
    variable rawmodes
    variable av_pow
    variable av_pow_i 
    variable l_orients_start
    variable known_flags

    #File Streams
    variable f_tvspik1
    variable f_tvsp
    variable f_tvsbl
    variable f_tvsflip
    variable f_tvsoop
    variable f_tvsstray
    variable f_tvsen
    variable f_tvsclust


    variable f_tvsdist
    

    # Averaging
    variable av_clust
    variable av_clust_i
    
    variable av_pik1
    variable av_pik1_i 
    
    variable av_pressure
    variable av_pressure_i
    
    variable av_boxl
    variable av_boxl_i 
    
    variable av_flip 
    variable av_flip_i 
    
    variable av_oop 
    variable av_oop_i 
    

    variable av_dist
    variable av_dist_i 


    variable av_components_en
    variable av_total_en
    variable av_kin_en
    variable av_fene_en
    variable av_harm_en
    variable av_nb_en
    variable av_en_i
    
    variable av_sizehisto
    variable av_sizehisto_i
    
    variable switches
    
    set av_sizehisto 0
    set av_sizehisto_i 0
    
    set av_clust 0
    set av_clust_i 0
    
    set av_pow 0
    set av_pow_i 0
    set av_pik1 0
    set av_pik1_i 0
    set av_pressure 0
    set av_pressure_i 0
    set av_boxl 0
    set av_boxl_i 0
    set av_flip 0
    set av_flip_i 0
    set av_oop 0
    set av_oop_i 0
    

    set av_dist 0
    set av_dist_i 0


    set av_components_en 0
    set av_total_en 0
    set av_kin_en 0
    set av_fene_en 0
    set av_harm_en 0
    set av_nb_en 0
    set av_en_i 0
    
    puts $switches	
    for { set i 0 } { $i < [llength $switches ] } { incr i } {
	puts [lindex $switches $i 0]
	puts -nonewline "Closing down: "
	switch [lindex $switches $i 0] {
	    "cluster_calc" {
		puts "cluster_calc"
		close $f_tvsclust
	    }
	    "pik1_calc" {
		puts "pik1_calc"
		unset all_particles
		close $f_tvspik1
	    }
	    "pressure_calc" {
		puts "pressure_calc"
		unset all_particles
		close $f_tvsp
	    }
	    "box_len_calc" {
		puts "box_len_calc"
		close $f_tvsbl 
	    }
	    "fluctuation_calc" {
		puts "fluctuation_calc"
		unset av_pow
	    }
	    "flipflop_calc" {
		puts "flipflop_calc"
		close $f_tvsflip
	    }
	    "orient_order_calc" {
		puts "orient_order_calc"
		close $f_tvsoop
	    }
	    "stray_lipids_calc" {
		puts "stray_calc "
		close $f_tvsstray
	    }
	    "energy_calc" {
		puts "energy_calc"
		close $f_tvsen 		    
	    }


	     "distance_calc" {
		puts "distance_calc"
		close $f_tvsdist 
	    }


	    "default" {
		mmsg::warn $this "unknown analysis flag [lindex $switches $i 0] $known_flags"
	    }
	}
	
    }
    unset switches
    mmsg::debug $this "done"
}






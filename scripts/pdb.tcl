proc writepsf {file} {
    set f [open $file "w"]
    puts $f "PSF"
    puts $f [format "%8d !NATOM" [setmd npart]]
    # write atoms and create bondlist
    set cnt 1
    set mp [setmd maxpart]
    set bondlist {}
    set bondcnt 0
    for {set p 0} { $p <= $mp } { incr p } {
	set tp [part $p p t]
	if { $tp != "na" } {
	    puts $f [format "%8d T%03d %4d UNX  FE   FE" \
			 $cnt $tp $p]
	    set count($p) $cnt
	    set bonds [part $p p b]
	    foreach b $bonds {
		set b [lindex $b 0]
		if {[llength $b ] == 2} {
		    incr bondcnt; lappend bondlist "{$cnt [lindex $b 1]}"
		}
	    }
	}
	incr cnt
    }
    #  write bonds
    puts $f [format "%8d !NBOND" $bondcnt]
    set bondlinecnt 0
    foreach b $bondlist {
	set b [lindex $b 0]
	if {[llength $b] == 2} {
	    incr bondcnt
	    eval "set p1 [lindex $b 0]"
	    eval "set p2 \$count([lindex $b 1])"
	    puts -nonewline $f [format "%8d%8d" $p1 $p2]
	    incr bondlinecnt
	    if {$bondlinecnt == 4} {
		puts $f ""
		set bondlinecnt 0
	    }
	}
    }
    close $f
}

proc writepdb {file {N_P -1} {MPC -1} {N_CI -1} {N_pS -1} {N_nS -1} } {
    set f [open $file "w"]
    puts $f "REMARK generated by tcl_md"
    # write atoms
    set mp [setmd maxpart]
    set cnt 0
    if { $N_P==-1 } {
	for {set p 0} { $p <= $mp } { incr p } {
	    set tp [part $p p t]
	    if { $tp != "na" } {
		set pos [part $p p p]
		puts $f [format "ATOM %6d  FE  UNX F%4d    %8.3f%8.3f%8.3f  0.00  0.00      T%03d" \
			     $cnt [expr $p % 10000] [lindex $pos 0] [lindex $pos 1] [lindex $pos 2] $tp]
		incr cnt
	    }
	}
    } else {
	# include topology information in the data stream
	set p 0
	for {set i 0} {$i < $N_P} {incr i} {
	    for {set j 0} {$j < $MPC} {incr j} {
		set tp [part $p p t]
		if { $tp != "na" } {
		    set pos [part $p p p]
		    puts $f [format "ATOM %6d  FE  %03d F%4d    %8.3f%8.3f%8.3f  0.00  0.00      T%03d" \
				 $cnt $i [expr $p % 10000] [lindex $pos 0] [lindex $pos 1] [lindex $pos 2] $tp]
		    incr cnt
		}
		incr p
	    }
	}
	set j1 [eval list [expr $N_P*$MPC] [expr $N_P*$MPC+$N_CI] [expr $N_P*$MPC+$N_CI+$N_pS]]
	set j2 [eval list [expr $N_P*$MPC+$N_CI] [expr $N_P*$MPC+$N_CI+$N_pS] [expr $N_P*$MPC+$N_CI+$N_pS+$N_nS]]
	foreach ja $j1 je $j2 {
	    for {set j $ja} {$j < $je} {incr j} {
		set tp [part $p p t]
		if { $tp != "na" } {
		    set pos [part $p p p]
		    puts $f [format "ATOM %6d  FE  %03d F%4d    %8.3f%8.3f%8.3f  0.00  0.00      T%03d" \
				 $cnt $i [expr $p % 10000] [lindex $pos 0] [lindex $pos 1] [lindex $pos 2] $tp]
		    incr cnt
		}
		incr p
	    }
	    incr i
	}
    }
    close $f
}

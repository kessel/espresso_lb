#!/usr/bin/tclsh8.4

#
# parse the output from the MEM_DEBUG feature.
# This script looks for the allocation, reallocation
# and freeing of memory locations to find possible
# double frees and memory leaks.
# Because it has to track a quite a few memory locations,
# it takes a while to run, usually longer than Espresso
# itself. Therefore this can only be used for really short
# runs.
#
# The script will output all location that got freed, but
# were not allocated before (or got freed twice), and the
# locations that never got freed, i.e. potential memory
# leaks.
###########################################################

set f [open [lindex $argv 0] "r"]
if { $argc > 1} {
    set n [lindex $argv 1]
} {
    set n 0
}

set cnt 0
while {![eof $f]} {
    if { [expr $cnt % 100] == 0 } { 
    	puts -nonewline stderr "."
    }
    incr cnt

    set line [gets $f]
    if {![regexp "^$n:" $line]} { continue }

    set line [string map {\" <} $line]

    if {[lindex $line 1] == "alloc"} {
	set size [lindex $line 2]
	set new [lindex $line 4]
	if {$new != "(nil)"} {
	    set allocated($new) "$size at [lindex $line 6]"
	}
    }
    if {[lindex $line 1] == "realloc"} {
	set old [lindex $line 2]
	set size [lindex $line 6]
	set new [lindex $line 4]
	if {$new != "(nil)"} {
	    if {$old != "(nil)"} {
		if { [array names allocated $old] != "" } {
		    set old_alloc ", from $allocated($old)"
	    	} {
		    set old_alloc ", from unmanaged source"
		}
	    } {
		set old_alloc ""
	    }
	    set allocated($new) "$size at [lindex $line 8] prev $old$old_alloc"
	}
	if { $old != "(nil)" && $old != $new } {
	    if { [array names allocated $old] != "" } {
		unset allocated($old)
	    }
	}
    }
    if {[lindex $line 1] == "free"} {
	set old [lindex $line 2]
	if {$old != "(nil)"} {
	    if { [array names allocated $old] != "" } {
		unset allocated($old)
	    } {
		puts "$old freed but never allocated"
	    }	
	}
    }
}

set pl ""
foreach {m f} [array get allocated] {
    set size [lindex $f 0]
    set file [lindex $f 2]
    set info [lrange $f 3 end]
    lappend pl "$file: $size@$m $info"
    if {[array get counts $file] != ""} {
	incr counts($file)
    } {
	set counts($file) 1
    }
}

set pl [lsort $pl]

foreach p $pl {
    puts $p
}

set l ""
foreach {f c} [array get counts] {
    lappend l "\# $f $c"
}
set l [lsort $l]
foreach x $l { puts $x }

# all.tcl --
#
# This file contains a top-level script to run all of the tests.

package require tcltest

if {[catch {
    tcltest::testsDirectory [file dir [info script]]
    tcltest::runAllTests
}]} {
    puts $errorInfo
}

puts "press enter to continue"
gets stdin
return

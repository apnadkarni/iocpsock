# all.tcl --
#
# This file contains a top-level script to run all of the tests.

package require tcltest

tcltest::testsDirectory [file dir [info script]]
tcltest::runAllTests

return

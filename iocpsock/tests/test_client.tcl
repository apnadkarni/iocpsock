console show

package require Iocpsock
set server {216.91.56.64}

proc TestClientRead {s i} {
    if {[string length [set e [fconfigure $s -error]]]} {
	#puts "error: $s: $e : closing socket"
	close $s
	return
    }
    if {![catch {read $s} reply]} {
        #puts "recieved: $reply"
	catch {close $s}
	return
    }
    if {[eof $s]} {
	#puts "closing"
	close $s
    }
}
proc TestClientWrite {s i} {
    if {[string length [set e [fconfigure $s -error]]]} {
	#puts "error: $s: $e at $i: closing socket"
	close $s
	return
    }
    catch {puts $s "hello?"}
    fileevent $s writable {}
}

proc connect {{howmany {1}}} {
    global server
    for {set a 0} {$a < $howmany} {incr a} {
	if {[catch {set s [socket2 -async $server 5150]} msg]} {
	    return "Barfed at $a with $msg"
	}
	fconfigure $s -blocking 0 -buffering none -translation binary
	fconfigure $s -sendpool 1 -recvburst 3
	fileevent $s readable [list TestClientRead $s $a]
	fileevent $s writable [list TestClientWrite $s $a]
	if {$a % 20} {update}
    }
    return
}

package require http

# reroute to my code.
http::register httpp 80 ::socket2

proc httpCallback {token} {
    global showme
    upvar #0 $token state
    # Access state as a Tcl array
#    puts [parray state]
    if {$state(status) != "ok"} {
	puts "error on $state(sock): $state(error): $state(http)"
    } else {
        if {$showme} {
	    set showme 0
	    puts $state(body)
	}
    }
    ::http::cleanup $state
    return
}

proc get {url {howmany 1}} {
    global showme
    set showme 1
    for {set a 0} {$a < $howmany} {incr a} {
	if {[catch {::http::geturl $url -command [list ::httpCallback] -blocksize 4096} msg]} {
	    puts "Barfed at $a with $msg"
	}
	if {($a % 40) == 0} {update}
    }
    return "done requesting."
}

proc cleanup {} {
    foreach f [file channels] {
	if {[string match iocp* $f]} {
	    close $f
	}
    }
}

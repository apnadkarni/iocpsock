catch {console show}

package require Iocpsock
set server 10.0.2.15

proc TestClientRead {s i} {
    if {[string length [set e [fconfigure $s -error]]]} {
	puts "error: $s: $e"
	close $s
	return
    }
    if {![catch {read $s} reply]} {
        if {$::debug} {
	    puts "received: $reply"
	}
	catch {close $s}
	return
    } else {
	if {$::debug} {
	    puts "got recv error: $reply"
	}
    }
    if {[eof $s]} {
	puts "closing?.. Why?"
	close $s
    } else {
	close $s
    }
}
proc TestClientWrite {s i} {
    if {[string length [set e [fconfigure $s -error]]]} {
        if {$::debug} {
	    puts "error: $s: $e at $i: closing socket"
	}
	close $s
	return
    }
    puts -nonewline $s "hello?"
    fileevent $s writable {}
}

proc connect {{howmany {1}}} {
    global server
    for {set a 0} {$a < $howmany} {incr a} {
	if {[catch {set s [socket2 -async $server 5150]} msg]} {
	    return "Barfed at $a with $msg"
	}
	fconfigure $s -blocking 0 -buffering none -translation binary
	fconfigure $s -sendcap 2 -recvmode {burst-detection 20 30}
	fileevent $s readable [list TestClientRead $s $a]
	fileevent $s writable [list TestClientWrite $s $a]
	if {$a / 20} {update}
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
    puts [parray state]
    if {$state(status) != "ok"} {
	puts "error on $state(sock): $state(error): $state(http)"
    } else {
        if {$showme} {
	    set showme 0
	    puts $state(body)
	}
    }
    ::http::cleanup $token
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

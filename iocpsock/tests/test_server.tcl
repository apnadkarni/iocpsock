#package require Iocpsock

set count 0
if {[info exist s]} {close $s}

proc accept {s addr port} {
    #puts "new connection from $addr:$port on $s"
    fconfigure $s -blocking 0 -buffering none -translation binary
    fileevent $s readable [list GotRead $s]
}
proc GotRead {s} {
    if {[string length [set e [fconfigure $s -error]]]} {
	puts "error: $s: $e : closing socket"
	close $s
	return
    }
    if {![catch {read $s}]} {
	catch {puts $s "hi there"}
    }
    if {[eof $s]} {
	#puts "closing $s"
	catch {close $s}
    }
}
set s [socket2 -server accept -myaddr [info hostname] 5150]
fconfigure $s -backlog 500 -sendcap 1 -recvburst 1

catch {console show}

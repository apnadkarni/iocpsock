package require Iocpsock

set count 0

proc accept {s addr port} {
    #global count
    #puts "new connection from $addr:$port on $s"
    #incr count
    #after 50
    close $s
}

set s [socket2 -server accept -myaddr [info hostname] 5150]
console show
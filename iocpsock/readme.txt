IOCPSOCK -- ver 2.0 a/k/a "The 'Got hardware speed?' release".
(6:50 PM 3/23/2004)

NEW for 2.0: tunable fconfigures.

http://sf.net/project/showfiles.php?group_id=73356
http://sf.net/projects/iocpsock

This is a new sockets channel driver for windows NT/2K/XP.  The aim is to
get it ready for inclusion in the core.  It's main difference from the
existing core tcp driver is the I/O model in use.  This uses
overlapped I/O with completion ports over the existing Win3.1 model
of WSAAsyncSelect.

It works great with tclhttpd.  As a server, the front-end just DOES NOT
get over-run.  In the performance tests I've run, I just can't get the
listening socket to ever fail (see below for the -backlog fconfigure).
With the stock channel driver, connect errors begin around 30 connections
per second.  Right now, I can do 80/sec with zero errors and push the test
tool well over that knee without incurring any errors.

Using a simple server and client script that exchange a simple "hello" and
"hello back" with the client doing the graceful shutdown, I'm able to do
1315 exchanges per second on a 2.4Ghz box running XP.  It does full
hardware speed (100mb/s) at around 35% CPU on the receiver.

Per socket resources are now tunable.  The new fconfigures are:

-backlog:	Sets the size of the AcceptEx pool.  Directly relates to
		the "bullet-proof" aspect of the front-end.  Under
		testing, I have found a setting of 700 to be impossible
		to ever cause a failure to a client connection (YMMV).
		Server socket only.  For query, it returns the cap and
		actual in-use as a list.

-sendcap:	Sets the limit of concurrent WSASend calls allowed on
		the socket or for new sockets if this is a server
		socket.  For query, it returns the cap and actual
		in-use in a list.

-recvburst:	Sets the cap allowed to the "(A)utomatic (B)urst
		(D)etection".  If set to one, ABD is turned off in favor
		of normal flow control.  For server sockets, this setting
		is copied to new connections.  For query, it returns the
		cap and actual in-use in a list.

To see ABD in action: http://iocpsock.sourceforge.net/netio.jpg

As the sender (on the left) increases it's -sendcap, thus allowing
a higher concurrent WSASend count, thus sending rate, the receiver
(on the right) enters a burst condition and increases its WSARecv
count to match the incoming flow.  Though not shown, turning off ABD
for normal flow-controlled behavior on the receiver will increase CPU
usage, but works well just the same.

There are essentially two modes one would operate a server under.
Either, one wants high throughput on few connections (FTPD) or a high
amount of short lived connections transfering very little (HTTPD).
The new fconfigures allow for both modes.

My suggestions are:

		FTPD		HTTPD
--------------------------------------
-backlog	10		500
-sendcap	20		1
-recvburst	20		1

I'm still experimenting with the tunable fconfigures and might decide
on different suggestions in the future.  Today, while testing the
original OS bug that this extension what written specifically to
overcome appears to have been fixed in a M$ service pack for XP!  I'll
try to find the root of this (my) confusion I'm having over the next
few days.

It's amazingly stable.  It provides one command called [socket2] and
behaves just like [socket], but with the added fconfigures.

It does IPv6, but is mostly untested in that respect.  The -sockname
and -peername fconfigures don't do IPv6 yet, though, but you can
create IPv6 sockets just by using an IPv6 address.

IMPORTANT things to know:

1) Don't set the -buffersize fconfigure to less than 4096.  This
limitation will be fixed in a future release.  Call me lazy..

2) The readable file event handler is only notified once per event
to read.  Because of my design for efficiency, no polling behavior
is used.  IOW, when the readable event handler is called (because
some bytes came in to the socket) and the script does not, for some
reason, call [read] or [gets] and no more bytes follow, the readable
event handler will not be called again (unless UpdateInterest() is
called by the generic layer, which is not a guarantee that it will do
so).  The same is true for the EOF condition.  If the readable event
handler does not close the socket for the [read] that returns an
empty string followed by an [eof] that returns true, the readable
event handler WILL NOT BE CALLED over and over until it does close
the socket unlike the core socket driver which will repeat unhandled
events.  FYI, the core socket repeats events because it is a polling
design, just look at SocketSetupProc() in win/tclWinSock.c and you'll
see that each iteration of the event loop must poll every socket
instance open to see if it is in a ready state.

My thoughts on this are simple..  Use a good script for the readable
handler that behaves properly and please.. always check for [eof]
after you [read] or [gets], not before.


* FAQ:

  Q: What's IOCP?
  A: A short for "I/O completion ports" and are combined with
     overlapped operations for the highest performance I/O model
     available on windows NT.

  Q: Ok, but what's "overlapped"?
  A: Posting a WSARecv, ConnectEx or AcceptEx call before the event
     (and data) arrives allowing the operation to happen wholly in
     kernel-mode so that not only do we get notification, but we get
     the data of the operation too.  Instead of "what's ready?", it's
     "here's what got done"

  Q: Gimme some links.  I want to read up on this.
  A: http://www.cswl.com/whiteppr/tech/rtime.html
     http://www.sysinternals.com/ntw2k/info/comport.shtml
     http://tangentsoft.net/wskfaq/articles/io-strategies.html
     http://msdn.microsoft.com/library/en-us/winsock/winsock/overlapped_i_o_2.asp

  Q: Does this only work on NT?
  A: Yes.  Just Win2K and WinXP.  I did try it once on NT4, though.  It
     can't work any of the Win9x flavors because completion ports are only
     an operating system feature of the NT flavors.


* CHANGES from 1.1:
  - new fconfigures: -backlog (in place of -ovrlpd), -sendcap, -recvburst.
  - A new -protocol option was added to [socket2], but will be used in
    future releases.
  - Graceful closing problems fixed.
  - all known bugs squashed.
  - now has a test suite.

* CHANGES from 1.0:
  - Use of the event loop made more efficient.  All polling behavior
    eradicated forever..  Pure feed-forward.  Now does blocking mode,
    too.

* CHANGES from 0.99:
  - [read] bug fixed where if the there was nothing to read, bytes read
    returned zero bytes rather than -1 with EWOULDBLOCK, oops.  Resulted
    in the generic layer thinking the connection gracefully closed.

* CHANGES from 0.5:
  - *ALL* resource leaks plugged.
  - Closing fixed.  It wasn't always closing the socket when it was
    initiating the shutdown of the connection.

* CHANGES from 0.4:
  - Now takes ipv6 addresses and creates ipv6 sockets when asked.
    WinXP can do ipv6 since sp1.  Win2K has a tech preview for ipv6,
    but isn't production quality.  Only when the address given to
    [socket2] is in ipv6 format does an ipv6 socket get used.  hostname
    might work, but untested.
  - Removed the puts to stderr on load.

* CHANGES from 0.3:
  - Properly works under threads.  Tcl_DeleteEventSource called in
    the threadexithandler and readyEvents linkedlist in the tsdPtr set
    to NULL so modifying it by sockets in the middle of closing will do
    nothing.
  - Removed the receive with accept optimization that AcceptEx does for
    more normal/expected behavior.  It's an additional trip through the
    completion routine, but I don't think it will take away any server
    speed as it doesn't wait for tcl anyways.

* CHANGES from 0.2:
  - Better tracking and clean-up of resources.  Memory use is tight.
  - All 'access violations' fixed.
  - Puts a load announcement to stderr (helps the author not make
    debugging mistakes).
  - Added a watchdog thread for cleaning-up mid-state connected peers.
    Part of the AcceptEx() optimization is that only when data is
    received on a newly accepted socket does the accept actually fire
    to the completion port and thus up to tcl.  If a connection is made
    and no data is sent for 2 minutes, this thread will close the
    connection to prevent a loss in resources.  This AcceptEx behavior
    is optimized for protocols who's connecting client starts the
    conversation -- HTTP would be an example.

* TODO:
  - Begin on UDP, IPX, IrDA, AppleTalk, DecNet, etc... support.
  - RAW sockets?  ICMP, IGMP?  multicast?  Hmm..
  - Add a trace feature for debugging purposes of internal behavior
    so odd things aren't silent.

--
David Gravereaux <davygrvy@pobox.com>

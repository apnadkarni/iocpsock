iocpsock.dll -- ver 1.0 a/k/a "it's done!" (10:36 PM 4/29/2003)

http://sf.net/project/showfiles.php?group_id=73356
http://sf.net/projects/iocpsock

This is a new sockets channel driver for windows NT.  The aim is to
get it ready for inclusion in the core.  It's main difference from the
existing core tcp driver is the I/O model in use.  This uses
overlapped I/O with completion ports over the existing Win3.1 model
of WSAAsyncSelect.  I haven't yet tested the all-out speed, but expect
a performance improvement of at least 4 times given the pipes and CPU
available.

It works great with tclhttpd, but its limits haven't been discovered
yet.  As a server, the front-end just DOES NOT get over-run.  In the
performance tests i've run, I just can't get the listening socket to
ever fail!  With the stock channel driver, connect errors begin around
30 connections per second.  Right now, I can do 80/sec with zero errors
and push it well over that.  How far above that I'm pushing it, I can't
tell as I need to build a better test environment next week to see,
but it flat-lines early.

It's fun in that the bottleneck is now tclhttpd (the script) itself
and user-mode CPU time handling sockets is now non-existent which lets
tclhttpd max-out at a lower input load.  Either sockets are 50% faster
or 33% more time is available to the script.  Either view seems valid.
In theory, 50K+ concurrent sockets are possible.  The only limitation
is available memory (~8K per socket of the non-paged pool is reserved,
which limits at about 1/4 of the physical memory: YMMV).

More development is needed.  It's amazingly stable.  Sockets are always
created in non-blocking mode.  There is *no* blocking mode yet.  This
will get fixed, eventually.  It provides one command called [socket2]
and behaves just like the stock one, except for the lack of blocking
mode.

It does IPv6, btw.  The -sockname and -peername fconfigures don't do
IPv6 yet, though, but you can create IPv6 sockets just by using an IPv6
address.

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
     "what's been done?"

  Q: Gimme some links.  I want to read up on this.
  A: http://www.cswl.com/whiteppr/tech/rtime.html
     http://www.sysinternals.com/ntw2k/info/comport.shtml
     http://tangentsoft.net/wskfaq/articles/io-strategies.html
     http://msdn.microsoft.com/library/en-us/winsock/winsock/overlapped_i_o_2.asp

  Q: Does this only work on NT?
  A: Yes.  Just Win2K and WinXP.  It might work on NT4, though.  It
     can't work any of the Win9x flavors because completion ports are
     an operating system feature of NT.

* CHANGES from 0.99:
  - [read] bug fixed where if the there was nothing to read, bytes read
    returned zero bytes rather than -1 with EAGAIN, oops.

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
  - Manage out-of-order receives so that additional completion threads can
    be used -- one per CPU is said to be most efficient.
  - Make the linkedlist routines waitable so blocking can be emulated.
  - Add special fconfigures for all the iocp stuff such as recv buffer
    size/count, accept buffer size/count and write concurrency.
  - Begin on UDP, IPX, IrDA, AppleTalk, DecNet, etc... support.
  - Test ConnectEx behavior on WinXP.

--
David Gravereaux <davygrvy@pobox.com>

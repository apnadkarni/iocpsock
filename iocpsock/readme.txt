iocpsock.dll -- ver 0.5 (Feb 18 13:01:50 2003)

http://sf.net/project/showfiles.php?group_id=73356
http://sf.net/projects/iocpsock

This is a new sockets channel driver for windows NT.  The aim is to
get it ready for inclusion in the core.  It's main difference from the
existing core tcp driver is the I/O model in use.  This uses
overlapped I/O with completion ports over the existing Win3.1 model
of WSAAsyncSelect.  I haven't yet tested the all-out speed, but expect
a performance improvement of at least 4 times given the pipes and CPU
available.

It works great with tclhttpd, but it's limits haven't been tested yet.

More development is needed.  Looks stable (much more than earlier).
Sockets are always created in non-blocking mode.  There is no blocking
mode yet.  This will get fixed, eventually.  It provides one command
called [socket2] and behaves just like the stock one.

* FAQ:

  Q: What's IOCP?
  A: A short for "I/O completion ports" and combined with overlapped
     operations for the highest performance I/O model available on
     windows NT.

  Q: Ok, but what's "overlapped"?
  A: Posting a WSARecv (or AcceptEx) call before the event (and data)
     arrives allowing the operation to happen wholly in kernel-mode so
     that not only do we get notification, but we get the data of the
     operation too.  Instead of "what's ready?", it's "what's been done"

  Q: Gimme some links.  I want to read up on this.
  A: http://www.cswl.com/whiteppr/tech/rtime.html
     http://www.sysinternals.com/ntw2k/info/comport.shtml
     http://tangentsoft.net/wskfaq/articles/io-strategies.html
     http://msdn.microsoft.com/library/en-us/winsock/winsock/overlapped_i_o_2.asp

  Q: Does this only work on NT?
  A: Yes.  Win2K and WinXP.  It might work on NT4, though.  It can't
     work any of the Win9x flavors because completion ports are an OS
     feature of NT.

* CHANGES from 0.4:
  - Now takes ipv6 addresses and creates ipv6 sockets when asked.  WinXP
    can do ipv6 since sp1.  Win2K has a tech preview for ipv6, but isn't
    production quality.  Only when the address given to [socket2] is in
    ipv6 format does an ipv6 socket get used.  hostname might work, but
    untested.
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

--
David Gravereaux <davygrvy@pobox.com>

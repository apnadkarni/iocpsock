iocpsock.dll -- ver 0.3 (Thu Feb 13 03:08:48 2003)

This is a new sockets channel driver for windows.  The aim is to get
it ready for inclusion in the core.  It's main difference from the
existing core tcp driver is the I/O model in use.  This uses
overlapped I/O with completion ports over the existing Win3.1 style
of WSAAsyncSelect.  I haven't yet tested the all-out speed, but expect
a performance improvement of at least 4 times given the pipes and CPU
available.

More development is needed.  Looks stable.  sockets are always created
in non-blocking mode.  There is no blocking mode yet.  This will get
fixed, eventually.

* FAQ:

  Q: What's IOCP?
  A: A short for "I/O completion ports" and combined with overlapped
     operations for the highest performance I/O model available on
     windows NT.

  Q: Ok, but what's "overlapped"?
  A: Posting a WSARecv (or AcceptEx) call before the event (and data)
     arrives allowing the operation to happen wholly in kernel-mode so
     that not only do we get notification, but we get the data of the
     operation too.

  Q: Gimme some links.  I want read up on this.
  A: http://www.cswl.com/whiteppr/tech/rtime.html
     http://www.sysinternals.com/ntw2k/info/comport.shtml
     http://tangentsoft.net/wskfaq/articles/io-strategies.html
     http://msdn.microsoft.com/library/en-us/winsock/winsock/overlapped_i_o_2.asp

* CHANGES from 0.2:
  - Better tracking and clean-up of resources.  Memory use is tight.
  - All 'access violations' fixed.
  - Puts a load announcement to stderr (helps the author not make
	debugging mistakes).
  - Added a watchdog thread for cleaning-up mid-state connected peers.
	Part of the AcceptEx() optimization is that only when data is
	received on a newly accepted socket does the accept actually
	fire to the completion port and thus up to tcl.  If a
	connection is made and no data is sent for 2 minutes, this
	thread will close the connection to prevent a loss in
	resources.  This AcceptEx behavior is optimized for protocols
	who's connecting client starts the conversation -- HTTP would
	be an example.

http://sf.net/projects/iocpsock

--
David Gravereaux <davygrvy@pobox.com>
iocpsock.dll -- ver 0.1 (12:48 PM 2/6/2003)

This is the first release of a new sockets channel driver for windows.
The aim is to get it ready for inclusion in the core.  It's main
difference from the existing core tcp driver is the I/O model in use.
This uses overlapped I/O with completion ports over the existing Win3.1
style of WSAAsyncSelect.  I haven't yet tested the all-out speed, but
expect a performance improvement of at least 4 times given the pipes
and CPU available.

More development is needed.  It seems stable enough to release.

http://sf.net/projects/iocpsock

--
David Gravereaux <davygrvy@pobox.com>
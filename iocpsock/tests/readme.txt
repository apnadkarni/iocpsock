these are some test scripts I was using.  This isn't a formal "test suite", but
could be useful.

test_server.tcl:  sets up a listener on port 5150, that just closes the socket
in the accept proc -- tests absolute acceptance speed.

test_client.tcl:  [connect] sends async connects to port 5150.  [get] with
httpp://...  (notice the extra 'p') does a blocking connect and async recieve
on the url.  Mostly used for testing clickstreams such as:

% get httpp://dave.lyris.com/tt?name=john 3000

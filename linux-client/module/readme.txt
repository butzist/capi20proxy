Documentation for the client module:
-----------------------------------

The main file is capiproxy.c.  This file provides as many as CAPI_MAXCONTR
(which at present is 16 -- kernelcapi) "virtual" ISDN cards.  It works like
this:
	- When the module is loaded it initializes all the virtual ISDN cards
	  and the internal application structures.
	- It calls attach_capi_driver() and waits for the daemons to call open().
	- A daemon is started (the client daemon)
	_ If specified, it spawns as many as CAPI_MAXCONTR processes which
	  call open() on the proxy device file.
	- The module calls attach_ctr() for each of these daemons and stores a
	  pointer to the corresponding "virtual device" (capi20proxy_card) 
	  structure in the private_data area of the file structure of the daemon.
	- When kernelcapi calls driver->register_appl(), such a message (i.e a
	  TYPE_REGISTER_APP message) is sent
	  to the daemon and the corresponding structure for that application id
	  given by kernelcapi (i.e APPL(applid) shall have been set to APPL_WAITING
	  to signify that the remote server has not yet registered it.

	  NB:
	  --
		By reading the kernelcapi sources (kcapi.c), it appears that kcapi
		determines the application id and so in effect sends an order to
		the driver to "register this application".  Thereafter, it sends an
		application identifier which *it* (kernelcapi) decided -- to the
		application that called capi_register() (this is on the client side
		of the whole picture.  This means that even 
		on the server end, the kernelcapi on the server
		shall dictate which application identifiers to use.  I suppose this
		shall imply that the daemon shall have to do some kind of mapping
		of application identifiers between the server and this module.

		The same applies to the controller numbers.  The numbers used by the
		server-side of the application are not necessarily used by the
		client side.
 
	_ The daemons should call "get_profile" once they register any applications
	  this information should be given to the module through the ioctl
	  IOCTL_SET_DATA for use incase a given application (on the client side)
	  calls capi_get_profile()
	- The daemon calls read() (which in this case is capiproxy_read()
	  and is put to sleep at this point if there
	  are no messages to send to the remote server
	- When kernel capi calls driver->send_message(), we queue the message
	  to be sent (in the card->outgoing_queue) and ask the kernel to schedule 
	  the sending of messages.
	- When the scheduler calls the handle_send_msg() function, all daemons with
	  pending messages are woken up, and they take the data to be sent
	  NB:
	  --
		Before any data is given to the daemon, we check the application
		id of the message and see whether the daemon managed to register
		the application on the remote server.  If not (APPL_REVOKED) the
		message is dropped (the daemon shall have sent an ioctl
		(IOCTL_ERROR_REGFAIL) -- this tells our card to call
		ctrl->appl_released() to tell kernel capi to get rid of that
		application.)

		If the status of the application is APPL_WAITING, the message is
		re-queued at the head of the queue and -EAGAIN is sent to the daemon
		(just like if it had set the O_NONBLOCK flag.)
	- The rest of the code should be fairly straightforward (please ask me in
	  case anything seems unusual -- it *could* be a bug!!)


	I hope the above documentation makes what I had in mind clearer.  I shall
	highly appreciate if you help me debug this piece of code.  I have not yet
	compiled it for I have just finished coding it (don't try to compile it yet
	;-))

	Thank you very much.

	Copyright (c) 2002
	Begumisa Gerald (beg_g@eahd.or.ug)
	All rights reserved.

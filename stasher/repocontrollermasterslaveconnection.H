/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef repocontrollermasterslaveconnection_H
#define repocontrollermasterslaveconnection_H

#undef libx_autogen_suppress_inner
#include "repocontrollermaster.H"

#include <x/mcguffinref.H>

//! Master controller's metadata for each connection to a slave

//! This is a mcguffin-based reference to a connection object to a slave.
//! The mcguffin is the repopeerconnectionbaseObj::peerlink, which is kept
//! on the connection thread's stack. When it goes out of scope, the
//! reference to the connection object gets dropped.
//!
//! An additional weak reference to the repopeerconnectionbaseObj::peerlink
//! is kept separately. repocontrollermasterObj::compute_quorum_get_peers()
//! sweeps the current list of peer connection objects, and removes this
//! entire object if the mcguffin-based reference is null(). However, when
//! a new connection object reports that it's now this master's slave,
//! the master initiates a link to the peer by first sending it a newly
//! constructed peerlink object, and a connect_master() message. The
//! peer connection thread, upon accepting the connection from this master
//! controller, invokes repocontrollermasterObj::accept(), and only when the
//! accept message gets handled, does the mcguffin-based reference to the
//! connection object gets installed. Therefore, for some period of time
//! the repopeerconnectionObj reference is not installed, but there is a
//! strong reference to the peerlink object (which is placed on the connection
//! thread's queue, and whose ownership is taken by the connection thread),
//! so this entire object should not get dropped if the mcguffin reference is
//! null, but the peerlink object still exists (pending connection).
//!
//! When the connection thread drops the peerlink object (either the thread is
//! stopped, or if the peer is now someone else's master), both the mcguffin-
//! based reference to the connection object, and the peerlink weak reference
//! become null.

class repocontrollermasterObj::slaveConnectionObj :
	public x::mcguffinrefObj<repopeerconnectionptr > {

	//! The master controller object.
	x::weakptr<repocontrollermasterptr> master;

public:
	//! Constructor
	slaveConnectionObj(const repocontrollermasterptr &masterArg);


	//! Destructor
	~slaveConnectionObj() noexcept;

	//! The peer linkage object that was sent to the thread. It's also the mcguffin.

	x::weakptr<repopeerconnectionbaseObj::peerlinkptr> link;

	//! The mcguffin-based reference has been destroyed

	//! Send a message to the master controller, requesting a quorum
	//! recalculation.
	void destroyed(const repopeerconnectionptr &) noexcept;
};

#endif

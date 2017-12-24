/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef slavesyncinfo_H
#define slavesyncinfo_H

#include "objrepocopydst.H"
#include "tobjrepo.H"
#include <x/weakptr.H>

class repocontrollerslaveObj;

//! Message to the peer connection object to begin syncing repo from master

//! This message is sent to a peer connection object to this node's master.
//! This message is sent when, from the repository controller's view, the
//! peer's status indicates that it's this node's master.
//!
//! When a slave repository controller starts, it has the connection object
//! to this node's master at hand, and it sends this message to the connection
//! object. When this message is received by the peer connection object's
//! execution thread, if the uuids don't match it must mean that the peer's
//! master has change, and the message gets quietly ignored. Otherwise, if the
//! uuids still amtch:
//!
//! - the peer connection thread retrieves \c dstinterface, and invokes its
//! start() method, using dstflag and mcguffin values from the handle. The
//! thread then removes the mcguffin reference from the handle, since the
//! destination copy thread is now the mcguffin's owner.
//!
//! - the beginsync message gets sent to the master, and the master replies with
//! a beginsyncack.
//!
//! - the connection object proceeds to forward repo sync messages
//! through dstinterface. The peer connection object keeps a reference
//! to this handle object while synchronization is in progress.
//!
//! \par If this node switches to a different master
//!
//! The \c dstinterface weak reference becomes a null reference. It's expected
//! that the peer connection object will be notified via statusupdated().
//!
//! When the peer connection object receives a statusupdated() indicating that
//! this node now has a different master, it drops its reference to this object
//! also.
//!
//! \par If the master peer is no longer a master peer
//!
//! When the peer connection object receives a \c nodeclusterstatus message
//! indicating that the peer has a different master, the peer connection object
//! drops its reference to this object.
//!
//! \par Synchronization complete
//!
//! After handing off a copycomplete message to the destination interface,
//! the peer connection object sets the destination interface reference to a
//! a null reference.

class slavesyncinfoObj : virtual public x::obj {

public:
	//! Constructor
	slavesyncinfoObj(const std::string &masternameArg,
			 const x::uuid &masteruuidArg,
			 const x::ptr<repocontrollerslaveObj> &controllerArg,
			 const objrepocopydstptr &dstinterfaceArg,
			 const tobjrepo &repoArg,
			 const boolref &dstflagArg,
			 const x::ptr<x::obj> &dstmcguffinArg)
		: mastername(masternameArg),
		  masteruuid(masteruuidArg),
		  controller(controllerArg),
		  dstinterface(dstinterfaceArg),
		  repo(repoArg),
		  dstflag(dstflagArg),
		  dstmcguffin(dstmcguffinArg) {}

	//! Destructor
	~slavesyncinfoObj() {}

	//! Master's name
	std::string mastername;

	//! Master's UUID
	x::uuid masteruuid;

	//! The controller object
	x::weakptr< x::ptr<repocontrollerslaveObj> > controller;

	//! The destination interface for a repository sync
	x::weakptr<objrepocopydstptr> dstinterface;

	//! The repository being synced to.

	tobjrepo repo;

	//! Parameter to the destination interface's start() method
	boolref dstflag;

	//! The mcguffin for the destination interface's start() method
	x::ptr<x::obj> dstmcguffin;
};

//! A reference to a slavesyncinfoObj

typedef x::ptr<slavesyncinfoObj> slavesyncinfo;

#endif

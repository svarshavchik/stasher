/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef repocontrollerbase_H
#define repocontrollerbase_H

#include <x/obj.H>
#include <x/uuid.H>
#include <x/logger.H>
#include <x/threads/run.H>
#include <string>

#include "repopeerconnectionfwd.H"
#include "repoclusterquorum.H"
#include "repocontrollerbasefwd.H"
#include "boolref.H"
#include "tobjrepofwd.H"
#include "stasher/quorumstateobj.H"
#include "stasher/results.auto.H"

//! Base class for repository controllers.

//! A repository controller gets started to manage this node's object
//! repository, as either the cluster's master, or another node's slave.
//!
//! This base class defines the common superclass interface for master and slave
//! controller instances.
//!
//! A controller must be explicitly started. After the controller object
//! gets constructed, it must not do anything until start_controller() gets
//! invoked.
//!
//! start_controller() receives a reference to an opaque object, a generic
//! mcguffin.
//! The controller must save a reference to the mcguffin, and keep the
//! reference in scope as long as the controller is properly operating. This
//! is a failsafe. If the reference to mcguffin ever goes out of scope,
//! Bad Things Will Happen.
//!
//! A controller remains in charge of the repository until it's instructed
//! to hand off the mcguffin to another controller, by invoking handoff().
//! The subclass should invoke handoff_next()
//! using the same mcguffin object that its own start_controller() received.
//! handoff_next() arranges so that this object's destructor invokes
//! the next controller's start_controller(). After handoff_next() returns,
//! this controller should relinquish the mcguffin and allow its own
//! reference go out of scope and terminate. [MASTERHANDOFF, SLAVEHANDOFF]
//!
//! Only the controller that's in possession of the mcguffin is deemed to have
//! the right to mess around with the object repository.
//!
//! The controller is expected to be implemented as a thread, that gets
//! started by start_controller(), and other methods are expected to be
//! implemented as
//! thread messages. Until start_controller() gets invoked, no action gets
//! taken for other
//! methods, they get queued up and processed after start_controller() gets
//! invoked.
//!
//! It is expected that start_controller()'s mcguffin gets placed on the
//!  executing thread's
//! stack, and after the thread invokes the handoff_next() method,
//! the thread can terminate. The executing thread's run() method can simply
//! leave its start argument alone on the stack.
//!
//! In this manner, any problem that results in a thread failure results in
//! Bad Things, and the handoff to another controller occurs only as part
//! of the previous controller's destruction.

class repocontrollerbaseObj : virtual public x::obj {

	//! The logger
	LOG_CLASS_SCOPE;

	//! Quorum callback notification list

	repoclusterquorum callback_list;

	//! A mcguffin that gets installed into quorumstatus_impl

	//! This mcguffin force a false quorum status to be reported immediately
	//! as soon as a new controller gets constructed, and before the
	//! previous controller terminates.
	//!
	//! The subclass's execution thread must invoke started() in order
	//! to clear the mcguffin and allow the new controller's quorum status
	//! to be reported.

	x::ptr<x::obj> quorumstatus_mcguffin;

protected:

	//! The controller has started

	//! This method clears the quorumstatus_mcguffin
	//!

	void started();

	//! Subclass announces that the quorum may have been reached or unreached
	void quorum(const STASHER_NAMESPACE::quorumstate &inquorum);


	//! Return the quorum status
	STASHER_NAMESPACE::quorumstate inquorum();

public:

	//! Return the current controller quorum status

	//! This sends a message to the controller thread, requesting that it
	//! return the current quorum status, from the context of the
	//! controller's thread.
	//!
	//! The controller thread sets the status parameter to the
	//! quorum's status, then the second boolref to true. The third
	//! parameter is a mcguffin.
	//!
	//! If the mcguffin goes out of scope (presuming no other references to
	//! it exist) and the processed flag is false, that means that the
	//! controller thread has stopped before it processed the message.

	virtual void get_quorum(const STASHER_NAMESPACE::quorumstateref &status,
				const boolref &processed,
				const x::ptr<x::obj> &mcguffin)=0;

	//! Return this controller's quorum value

	//! For debugging purposes only, in context where the controller thread
	//! is not expected to terminate.
	//!
	//! Calls get_quorum(), and waits for a response.

	STASHER_NAMESPACE::quorumstate debug_inquorum();

	//! Master node name, either this node or another on in the cluster.
	const std::string mastername;

	//! Master instance uuid
	const x::uuid masteruuid;

	//! The object repository
	const tobjrepo repo;

	//! Constructor
	repocontrollerbaseObj(//! Master node name, either this node or another on in the cluster.
			      const std::string &masternameArg,

			      //! Master instance uuid
			      const x::uuid &masteruuidArg,

			      //! The object repository
			      const tobjrepo &repoArg,

			      //! Quorum callback notification list
			      const repoclusterquorum &callback_listArg);


	//! Destructor
	~repocontrollerbaseObj() noexcept;

	//! start_controller() starts a thread. This is what it returns.

	//! The reason that it's a ptr is that if the stoppable thread tracker
	//! is no longer starting threads, start() returns a null ptr.

	typedef x::runthreadptr<void> start_controller_ret_t;

	//! The controller can start now.

	//! start_controller() gets a reference to an opaque object, a mcguffin.
	//! The controller must release this mcguffin when it's done.

	virtual start_controller_ret_t
	start_controller(const x::threadmsgdispatcherObj::msgqueue_obj &msgqueue,
			 const x::ref<x::obj> &mcguffin)=0;

	//! The controller can stop now, and start the next controller.

	virtual void handoff( //! Start this controller, pass along the mcguffin

			     const repocontroller_start_info &next)=0;


	//! A peer has switched to a new master

	virtual void peernewmaster(//! Existing peer
				   const repopeerconnectionptr &peerRef,
				   //! Its new master
				   const nodeclusterstatus &peerStatus)=0;


	//! Request the master to hand itself off to some other peer

	virtual x::ptr<x::obj>
	handoff_request(const std::string &peername)=0;

	//! An admin request to hald the entire cluster

	//! An admin connection invokes this, passing a results object, where
	//! the status of the request goes, and a mcguffin. When the mcguffin
	//! goes out of scope, the results objects gets sent as an ack, to the
	//! requesting client.
	//!
	//! If this is a slave controller, the request gets declined with an
	//! error message. A master controller has some work to do, to wind
	//! everything down.

	virtual void halt(//! Result of the request, to be determined
			  const STASHER_NAMESPACE::haltrequestresults &req,

			  //! Mcguffin.
			  const x::ref<x::obj> &mcguffin)=0;

protected:
	//! Subclass invokes handoff_next() when handoff is requested.

	//! subclass should then relinquish its reference to the mcguffin.
	//!
	void handoff_next(//! Next controller to start
			  const repocontroller_start_info &next,

			  //! Mcguffin to pass along
			  const x::ref<x::obj> &mcguffin);

private:
	//! Controller reference saved by handoff_next()
	repocontroller_start_infoptr next_controller;

	//! Mcguffin reference saved by handoff_next()
	x::ref<x::obj> next_controller_mcguffin;
};


#endif

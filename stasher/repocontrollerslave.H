/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef repocontrollerslave_H
#define repocontrollerslave_H

#include <x/weakptr.H>
#include <x/logger.H>
#include "stasher/quorumstate.H"
#include "repocontrollerbasehandoff.H"
#include "repopeerconnectionbase.H"
#include "boolref.H"
#include "threadreportimpl.H"
#include "trandistreceivedfwd.H"
#include "objrepocopydstfwd.H"
#include "tranreceivedfwd.H"

class peerstatusObj;
class trandistributorObj;

//! Slave repository controller

//! When the controller starts, it invokes the peer connection object's
//! connect_slave(), passing it a
//! \link slavesyncinfoObj slavesyncinfo\endlink.
//!
//! It's expected that the slave controller object gets instantiated when
//! there's a connection object present to a master peer. It's expected that
//! in the event that this node switches to a new master, or the peer stops
//! being a master, handoff gets invoked() to stop the thread, which also
//! destroys the destination object repository copy thread.
//!
//! get_quorum() returns \c true once the repository has been synchronized.
//! When the controller starts, it spawns off an
//! \ref objrepocopydstObj "destination object repository copy" thread object.
//! When its mcguffin gets destroyed, the \c boolref indicates whether or
//! not the repository copy operation succeeded, if so get_quorum() will then
//! return true, and a quorum is announced(). [SLAVEQUORUM]

class repocontrollerslaveObj : public repocontrollerbasehandoffObj,
			       public STASHER_NAMESPACE::threadreportimplObj {

#include "repocontrollerslave.msgs.H"

private:
	//! The logger object

	LOG_CLASS_SCOPE;

	//! Connection to the master

	x::weakptr<repopeerconnectionbase> peer;

	//! Repository copy thread, on the executing thread's stack
	objrepocopydstptr *dstcopy_ptr;

	//! Notify master of transaction received by this slave.

	//! Valid on the executing thread's stack.
	//!

	trandistreceivedptr *receiver_ptr;

	//! Set once the repository copy thread terminates succesfully.
	bool synccomplete_flag;

	//! Last quorum state announcement received from master

	STASHER_NAMESPACE::quorumstate lastmasterquorumstate;

	//! The distributor -- may be NULL for some regression tests
	x::weakptr<x::ptr<trandistributorObj> > distributor;

	//! Invoke this stop() after master orders me to do this.

	x::stoppable haltstop;

	class dstcopyObj;

public:

	//! Inherited, send a do_get_quorum() message.

	void get_quorum(const STASHER_NAMESPACE::quorumstateref &status,
			const boolref &processed,
			const x::ptr<x::obj> &mcguffin) override;

	//! Constructor
	repocontrollerslaveObj(//! Master node name, either this node or another on in the cluster.
			       const std::string &masternameArg,

			       //! Peer connection object to the master
			       const repopeerconnectionbase &peerArg,

			       //! Master instance uuid
			       const x::uuid &masteruuidArg,

			       //! The object repository
			       const tobjrepo &repoArg,

			       //! Quorum callback notification list
			       const repoclusterquorum &callback_listArg,

			       //! The distributor object

			       //! Some regression tests may pass a NULL
			       //! reference, which disables linkage with the
			       //! distributor.
			       const x::ptr<trandistributorObj> &distributorArg,

			       //! The thread tracker that starts this thread
			       const STASHER_NAMESPACE::stoppableThreadTracker &trackerArg,

			       //! Invoke this stop after my master ordered me to
			       const x::stoppable &haltstopArg);


	//! Destructor
	~repocontrollerslaveObj() noexcept;

	/* Externally-invoked methods */

	//! A slave controller does not use this, a no-op

	void peernewmaster(const repopeerconnectionptr &peerRef,
			   const nodeclusterstatus &peerStatus) override;


	//! Retrieve the connection object, for debugging purposes

	repopeerconnectionbase debugGetPeer()
	{
		return peer.getptr();
	}

	//! Request the master to hand itself off to some other peer

	x::ptr<x::obj>
	handoff_request(const std::string &peername) override;

	//! Start this thread

	start_controller_ret_t start_controller(const msgqueue_obj &msgqueue,
						const x::ref<x::obj> &mcguffin)
		override;

	//! The thread
	void run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
		 const msgqueue_obj &msgqueue,
		 x::ref<x::obj> &start_arg);

	void foo() override {}

private:

	//! Initialize connection with a slave

	void initialize(const repopeerconnectionbase &peerRef,
			const x::ptr<x::obj> &thread_mcguffin);

	//! Report this thread's status

	std::string report(std::ostream &rep);

	//! Implement a halt request, by saying that we aren't the master.

	void halt(const STASHER_NAMESPACE::haltrequestresults &req,
		  const x::ref<x::obj> &mcguffin) override;
};

#endif

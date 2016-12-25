/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef batonhandoverthread_H
#define batonhandoverthread_H

#include <x/threads/run.H>
#include <x/threadmsgdispatcher.H>
#include <x/logger.H>

#include "boolref.H"
#include "repoclusterquorum.H"
#include "clusterinfofwd.H"

//! Baton handover monitoring thread

//! This thread is started to monitor the handover of a baton from this node
//! to another node.
//!
//! Invoke create_startarg(), and use the object it returns as a starting
//! argument for this thread.
//!
//! This thread takes a mcguffin created when the baton transfer gets initiated
//! on this master, and runs until the mcguffin gets destroyed, and until a
//! quorum gets established, with the expectation that the mcguffin's
//! destruction indicates either a success in getting this master to hand over
//! the baton to the new master, or if the transfer failed. When the mcguffin
//! gets destroyed and the quorum is present, this thread checks whether the
//! current cluster master is the requested new master, then terminates.
//!
//! The starting argument to this thread includes the requested new master's
//! name, and a flag. This thread object may be used as a mcguffin. When this
//! thread terminates and this object gets destroyed because there are no
//! further references to it, the flag's status indicates the result of the
//! comparison between the current cluster's master, and the requested master
//! name [BATONHANDOVERTHREAD].

class batonhandoverthreadObj : public x::runthreadname,
			       virtual public x::threadmsgdispatcherObj {

	//! The logger

	LOG_CLASS_SCOPE;

	class baton_destroyed_callback;
	class quorum_status_callback;

	//! The requested new master node's name, on this thread's stack.
	const std::string *newmaster;

	//! The transfer status object reference, on this thread's stack.
	const boolref *completed_status;

	//! This node's status, on this thread's stack
	const clusterinfo *cluster;

	//! Flag: the baton has been destroyed
	bool baton_destroyed_flag;

	//! Flag: current quorum status
	bool quorum_flag;

	//! Return the thread's name
	std::string getName() const;
public:

	//! Constructor
	batonhandoverthreadObj();

	//! Destructor
	~batonhandoverthreadObj() noexcept;

	//! The thread
	void run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
		 const std::string &newmasterref,
		 const boolref &completed_statusref,
		 x::weakptr<x::ptr<x::obj> > &baton_handedover,
		 const x::ptr<repoclusterquorumObj> &clusterquorum,
		 const clusterinfo &clusterref);

private:

	//! If the baton handover mcguffin has been destroyed, and the cluster is in quorum, check the cluster's state, and stop.

	void check_transfer_completed();

#include "batonhandoverthread.msgs.H"

};


#endif

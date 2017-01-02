/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef repocontrollerbasehandoff_H
#define repocontrollerbasehandoff_H

#include "repocontrollerbase.H"
#include "stoppablethreadtracker.H"
#include <x/threadmsgdispatcher.H>

//! Implement start_controller() and handoff() using message dispatching.

//! The subclass must set the \c mcguffin defined here to point to the mcguffin
//! object reference on its stack, which it receives as the run argument.
//!
//! The subclass also implements start_controller() to pass
//! the mcguffin as the run argument (which the executing thread must
//! set the \c mcguffin to point to. handoff() sends a handoff message to
//! the executing thread, and a dispatch() method is provided to invoke
//! handoff_next(), the stop().

class repocontrollerbasehandoffObj : public repocontrollerbaseObj,
				     virtual public x::threadmsgdispatcherObj {

#include "repocontrollerbasehandoff.msgs.H"

protected:

	//! The thread tracker that starts this thread

	STASHER_NAMESPACE::stoppableThreadTracker tracker;

	//! Pointer to the controller mcguffin on the executing thread's stack.

	x::ref<x::obj> *mcguffin;


public:
	//! Constructor

	repocontrollerbasehandoffObj(//! Master node name, either this node or another on in the cluster.
				     const std::string &masternameArg,

				     //! Master instance uuid
				     const x::uuid &masteruuidArg,

				     //! The object repository
				     const tobjrepo &repoArg,

				     //! Quorum callback notification list
				     const repoclusterquorum &callback_listArg,

				     //! The thread tracker that starts this thread

				     const STASHER_NAMESPACE::stoppableThreadTracker &trackerArg);


	//! Destructor
	~repocontrollerbasehandoffObj();

	//! Wrapper for handoff()

	void handoff(const repocontroller_start_info &next)
		override;
};


#endif

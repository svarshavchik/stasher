/*
** Copyright 2016-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef repocontrollerbasefwd_H
#define repocontrollerbasefwd_H

#include <x/ref.H>
#include <x/obj.H>
#include <x/threadmsgdispatcher.H>

class nodeclusterstatus;
class repocontrollerbaseObj;

//! A reference to a repocontrollerbaseObj

typedef x::ref<repocontrollerbaseObj> repocontrollerbase;

//! What's needed to start a controller

class repocontroller_start_infoObj : virtual public x::obj {

public:
	//! The new controller waiting to be started.
	const repocontrollerbase new_controller;

	//! Its message queue
	const x::threadmsgdispatcherObj::msgqueue_obj new_controller_queue;

	//! Constructor.

	//! Takes the new controller as a parameter, and creates a
	//! new message queue for it.
	repocontroller_start_infoObj(const repocontrollerbase &new_controller);

	//! Destructor
	~repocontroller_start_infoObj();

	//! Start the new controllers
	void start(const x::ref<x::obj> &mcguffin);
};

//! The new master or slave controller

//! A new controller object gets created as the first step of switching
//! to a new master or a slave controller. However, its execution thread
//! does not get started right away, the existing controller must commit
//! suicide, first.
//!
//! create_master_controller()/create_slave_controller() constructs the
//! controller object, then creates this repocontroller_start_info handle.
//! The handle then gets passed in the handoff() message to the existing
//! controller. handoff() calls handoff_next(), which stores the new
//! repocontroller_start_info as the next_controller, then stop()s the
//! thread.
//!
//! repocontrollerbase's destructor checks if next_controller has been set,
//! and, if so, invokes repo_controller_start_info's start(), to start the
//! execution thread.
//!
//! If this is the first controller, startmaster() is responsible for
//! invoking start() right waay.

typedef x::ref<repocontroller_start_infoObj> repocontroller_start_info;

//! \see repocontroller_start_info

typedef x::ptr<repocontroller_start_infoObj> repocontroller_start_infoptr;

#endif

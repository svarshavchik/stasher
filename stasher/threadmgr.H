/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef threadmgr_H
#define threadmgr_H

#include <x/stoppable.H>
#include <x/obj.H>
#include <x/weakptr.H>
#include <x/mcguffinref.H>
#include <x/threads/run.H>
#include <x/threadmsgdispatcher.H>
#include <type_traits>

//! Additional parameter to threadmgr-managed threads.

//! A \c start_threadmsgdispatcher_sync parameter gets passed to all threadmgr-managed
//! threads. After instantiating a msgqueue_auto, and clearing the
//! x::threadmsgdispatcher's mcguffin, the thread must invoke
//! thread_started().
//!
//! This blocks the execution thread until threadmgr's start_threadmsgdispatcher() finishes
//! initializing itself.

class start_threadmsgdispatcher_syncObj : virtual public x::obj {

public:

	//! Synchronization mutex
	std::mutex mutex;

	//! Constructor
	start_threadmsgdispatcher_syncObj();

	//! Destructor
	~start_threadmsgdispatcher_syncObj();

	//! Invoked in the threadmgr-managed execution thread.
	void thread_started();
};

typedef x::ref<start_threadmsgdispatcher_syncObj> start_threadmsgdispatcher_sync;

//! Manage a object run()ing a stoppable thread.

//! A subclass typically inherits from this template class, and starts a thread.
//! If the thread is still running, the destructor stop()s it, and wait()s for
//! the thread to finish, before returning from the destructor.
//!
//! The template parameter is either a ref or a ptr to the object that runs
//! the thread.

template<typename ref_type>
class threadmgrObj : public x::stoppableObj, virtual public x::obj {

	//! Define the type of a mcguffin-based reference to the thread

	//! The runthread object returned by x::run() gets used as a mcguffin
	//! to store the reference to the running thread's object. So, when
	//! the thread terminates, that releases theradmgrObj's reference on the
	//! thread object, automatically.
	typedef x::ref<x::mcguffinrefObj<x::ptr<typename ref_type::obj_type,
						typename ref_type::base> >
		       > thr_t;

	//! The running thread.
	thr_t thr;

	//! The running thread's mcguffin, its return object, really.
	x::weakptr<x::runthreadbaseptr> threadret;

public:
	//! Constructor
	threadmgrObj() : thr(thr_t::create()) {}

	//! If the thread is still running stop()it, then wait() for it to stop.
	~threadmgrObj()
	{
		try {
			stop();
			wait();
		} catch (...) {
		}
	}

	//! Start a thread.

	//! Start a thread.

	//! Invoke the thread's run() method using x::start_threadmsgdispatcher().
	//! If the thread is still running by the time this object gets
	//! destroyed, it gets stopped.

	template<typename ...Args>
	void start_threadmsgdispatcher(const x::ref<typename ref_type::obj_type,
			  typename ref_type::base> &thrArg,
			  Args && ...args)
	{
		// Create an additional start_threadmsgdispatcher_sync parameter that the
		// thread gets. The thread calls its thread_started() method
		// that attempts to acquires its mutex. We acquires the
		// mutex first, here. This blocks the execution thread until
		// we finish initializing threadret.

		auto sync_arg=start_threadmsgdispatcher_sync::create();
		std::unique_lock<std::mutex> sync_lock{sync_arg->mutex};

		x::runthreadbase ret=x::start_threadmsgdispatcher(thrArg,
						     sync_arg,
						     std::forward<Args>(args)...);
#ifdef THREADMGR_DEBUG_HOOK
		THREADMGR_DEBUG_HOOK;
#endif
		threadret=ret;

		thr->install(thrArg, ret);
	}


	//! Check if the thread is still running.

	//! \return \c true - the thread is still running,
	//! \c false otherwise.

	bool running() const
	{
		std::lock_guard<std::mutex> lock(objmutex);

		return (!threadret.getptr().null());
	}

	//! Stop the managed thread.

	void stop()
	{
		auto s=thr->getptr();

		if (!s.null())
			s->stop();
	}


	//! Wait until the thread terminates.

	//! The wait() function does not return until the thread terminates.

	void wait() const
	{
		auto s=({
				std::lock_guard<std::mutex> lock(objmutex);

				threadret.getptr();
			});

		if (s.null())
			return;

		s->wait();
	}

	//! Pass through event() messages to the thread

	template<typename msg_type>
	void event(const msg_type &object)
	{
		auto s=thr->getptr();

		if (!s.null())
			s->event(object);
	}
};

#endif

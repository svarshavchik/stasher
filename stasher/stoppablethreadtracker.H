/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stoppablethreadtracker_H
#define stoppablethreadtracker_H

#include <x/ref.H>
#include <x/obj.H>
#include <x/weaklist.H>
#include <x/weakmultimap.H>
#include <x/exception.H>
#include <x/threads/run.H>
#include <x/logger.H>

#include <x/stoppable.H>
#include "stasher/namespace.H"
#include "stoppablethreadtrackerfwd.H"

STASHER_NAMESPACE_START

class threadreportObj;

//! Stop stoppable threads upon request.

//! start_thread() saves a weak reference to the given thread, and invokes
//! the thread object's start() method, to get it going. [START]
//!
//! The thread object must also be derived from x::stoppableObj. [STOPPABLE]
//!
//! stop_threads() invokes stop() on all saved thread objects, then
//! waits for each thread to stop, before returning. [STOPPING]
//!
//! Invoking start_thread() after stop_threads() was already invoked does
//! nothing, the thread object's start() method does not get invoked.
//! [ONESHOT]
//!
//! The destructor invokes stop_threads(). // [DESTRUCTOR]
//!
//! This is actually a facade for an instance of the implObj inner class,
//! which gets instantiated by the constructor, so when this object gets
//! destroyed, stop_threads() gets invoked for the underlying inner class.

class stoppableThreadTrackerImplObj : virtual public x::obj {

	//! The logger object

	LOG_CLASS_SCOPE;

	//! The actual implementation object

	//! stoppableThreadTrackerImplObj maintains a reference to this object.
	//! When stoppableThreadTrackerImplObj goes out of scope, stop_threads()
	//! gets invoked.

	class implObj : virtual public x::obj {

		//! The container for the list of started threads

		//! Each thread's id is stored as a key.

		typedef x::weakmultimap<std::thread::id,
					x::stoppableObj> thread_list_t;

		//! A list of started stoppables

		thread_list_t thread_list;

		//! A container for the list of thread objects still hanging around, to wait for.
		typedef x::weaklist<x::runthreadbase::obj_type>
		running_threads_t;

		//! List of thread objects still hanging around, to wait for.
		running_threads_t running_threads;

		//! Flag: stop_threads() has been invoked.

		bool stopping;

	public:
		//! Constructor
		implObj();

		//! Destructor
		~implObj() noexcept;

		//! Stop active threads.

		//! Invoked from stoppableThreadTrackerImplObj's destructor

		void stop_threads(//! The default of \c true stops all current threads,
				  //! and prevents any more threads from being started.
				  //! Set to \c false to stopp all current threads,
				  //! but allow ongoing thread instantiation.
				  bool permanently=true);

		//! Start a thread.

		template<typename objClass, typename baseClass,
			 typename ...Args>
		auto start(const x::ref<objClass, baseClass> &thread,
			   Args && ...args)
			-> typename x::runthreadptr<
				decltype(thread->run(x::invoke_run_param_helper
						     <Args>()...))
					>
		{
			// Make sure the thread object implements the stoppable
			// interface
			x::stoppableObj &s= *thread;

			std::lock_guard<std::mutex> lock(objmutex);

			if (stopping)
				return typename x::runthreadptr<
					decltype(thread->run
						 (x::invoke_run_param_helper
						  <Args>()...))>();

			auto ret=x::run(thread, std::forward<Args>(args)...);

			thread_list->insert(std::make_pair(ret->get_id(),
							   x::stoppable(&s)));
			running_threads->push_back(ret);
			return ret;
		}

		//! Request a report from all the running threads
		x::ptr<threadreportObj> getReport();
	};

public:
	//! A reference to the actual implementation
	typedef x::ref<implObj> impl;

	//! A nullable reference pointer to the actual implementation
	typedef x::ptr<implObj> implptr;

private:

	//! The implementation object

	impl implref;

public:

	//! Constructor
	stoppableThreadTrackerImplObj();

	//! Destructor
	~stoppableThreadTrackerImplObj() noexcept;

	//! Invoke implObj::stop_threads()

	void stop_threads(//! The default of \c true stops all current threads,
			  //! and prevents any more threads from being started.
			  //! Set to \c false to stopp all current threads,
			  //! but allow ongoing thread instantiation.
			  bool permanently=true);

	//! Invoke implObj::start()

	template<typename objClass, typename baseClass,
		 typename ...Args>
	auto start(const x::ref<objClass, baseClass> &thread,
		   Args && ...args)
		-> typename x::runthreadptr<
			decltype(thread->run(x::invoke_run_param_helper<Args>()
					     ...))>
	{
		return implref->start(thread, std::forward<Args>(args)...);
	}

	//! Return a reference to a tracker object that uses implObj

	x::ref<stoppableThreadTrackerObj> getTracker();
};

//! A facade for a stoppableThreadTrackerImplObj that uses a weak reference

//! This facade implements just start_thread() through a weak reference to
//! the stoppableThreadTrackerImplObj. A reference
//! to this object gets returned by stoppableThreadTrackerImplObj::getTracker().
//! This reference may be freely passed around wherever threads
//! need to be started, without impacting the reference count of the
//! actual thread tracker, the stoppableThreadTrackerImpl instance.
//!
//! start_thread() passes through to the underlying stoppableThreadTrackerImpl
//! object through a weak pointer [PASSTHRU].
//!
//! When that object goes out of scope, and the weak pointer nulls out,
//! calls to start_thread() become no-ops. [NOOP]

class stoppableThreadTrackerObj : virtual public x::obj {

	//! The weak reference to the real implementation.

	x::weakptr<stoppableThreadTrackerImplObj::implptr> implref;

public:
	//! Constructor.
	stoppableThreadTrackerObj(const stoppableThreadTrackerImplObj::impl
				  &implrefArg);


	//! Destructor
	~stoppableThreadTrackerObj() noexcept;

	//! Start a thread, unless we're stopping all the threads.

	template<typename objClass, typename baseClass,
		 typename ...Args>
	auto start(const x::ref<objClass, baseClass> &thread,
		   Args && ...args)
		-> typename x::runthreadptr<
			decltype(thread->run(x::invoke_run_param_helper<Args>()
					     ...))>
	{
		stoppableThreadTrackerImplObj::implptr s(implref.getptr());

		if (!s.null())
			return s->start(thread, std::forward<Args>(args)...);

		return typename x::runthreadptr<
			decltype(thread->run(x::invoke_run_param_helper<Args>()
					     ...))
				>();
	}

	//! Request a report from all the running threads
	x::ptr<threadreportObj> getReport();

};

STASHER_NAMESPACE_END


#endif

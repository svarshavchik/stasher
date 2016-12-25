/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "stoppablethreadtracker.H"
#include "threadreport.H"
#include "threadreportimpl.H"

LOG_CLASS_INIT(STASHER_NAMESPACE::stoppableThreadTrackerImplObj);

STASHER_NAMESPACE_START

stoppableThreadTrackerImplObj::stoppableThreadTrackerImplObj()
	: implref(impl::create())
{
}

stoppableThreadTrackerImplObj::~stoppableThreadTrackerImplObj() noexcept
{
	try {
		stop_threads();
	} catch (...)
	{
		LOG_FATAL("Exception caught in the destructor");
	}
}

void stoppableThreadTrackerImplObj::stop_threads(bool permanently)

{
	implref->stop_threads(permanently);
}

stoppableThreadTracker stoppableThreadTrackerImplObj::getTracker()

{
	return stoppableThreadTracker::create(implref);
}


stoppableThreadTrackerImplObj::implObj::implObj()
	: thread_list(thread_list_t::create()),
	  running_threads(running_threads_t::create()),
	  stopping(false)
{
}

stoppableThreadTrackerImplObj::implObj::~implObj() noexcept
{
}

void stoppableThreadTrackerImplObj::implObj::stop_threads(bool permanently)

{
	std::list<x::stoppable> list;

	{
		std::lock_guard<std::mutex> lock(objmutex);

		if (permanently)
			stopping=true;

		for (auto thread: *thread_list)
		{
			x::stoppableptr s(thread.second.getptr());

			if (!s.null())
				list.push_back(s);
		}
	}

	LOG_INFO("Stopping " << list.size() << " threads");
	while (!list.empty())
	{
		x::stoppable s(list.front());

		list.pop_front();
		s->stop();
	}

	for (auto tptr: *running_threads)
	{
		auto t=tptr.getptr();

		if (!t.null())
			t->wait();
	}

	LOG_INFO("All threads stopped");
}

x::ptr<threadreportObj> stoppableThreadTrackerImplObj::implObj::getReport()

{
	x::ptr<threadreportObj> rep=x::ptr<threadreportObj>::create();

	rep->mcguffin=x::ptr<x::obj>::create();

	std::lock_guard<std::mutex> lock(objmutex);

	rep->reports.reserve(thread_list->size());

	for (auto thread: *thread_list)
	{
		x::stoppableptr s(thread.second.getptr());

		if (s.null())
			continue;

		x::ref<singlethreadreportObj> r=
			x::ref<singlethreadreportObj>::create();

		rep->reports.push_back(r);

		r->thread_id=thread.first;

		threadreportimplObj *trio=
			dynamic_cast<threadreportimplObj *>(&*s);

		if (trio)
		{
			trio->report(r, rep->mcguffin);
			continue;
		}

		// Try several fallbacks
		r->name=s->objname();

		x::runthreadname *thr=dynamic_cast<x::runthreadname *>(&*s);

		if (thr)
			r->report=thr->getName();
		else
			r->report="No report available";
	}

	return rep;
}

// ---

stoppableThreadTrackerObj
::stoppableThreadTrackerObj(const stoppableThreadTrackerImplObj::impl &implArg)
	: implref(implArg)
{
}

stoppableThreadTrackerObj::~stoppableThreadTrackerObj() noexcept
{
}

x::ptr<threadreportObj> stoppableThreadTrackerObj::getReport()

{
	stoppableThreadTrackerImplObj::implptr s(implref.getptr());

	if (!s.null())
		return s->getReport();

	// Punt

	x::ptr<threadreportObj> rep=x::ptr<threadreportObj>::create();

	rep->mcguffin=x::ptr<x::obj>::create();

	return rep;
}

STASHER_NAMESPACE_END

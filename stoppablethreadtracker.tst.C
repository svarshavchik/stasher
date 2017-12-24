/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "stoppablethreadtracker.H"
#include <x/threadmsgdispatcher.H>
#include <x/options.H>

class dummyThreadObj;

class dummyThreadObj : public x::threadmsgdispatcherObj {
	// [STOPPABLE]

public:

	dummyThreadObj()
	{
	}

	~dummyThreadObj()
	{
	}

	void run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
		 const x::ptr<x::obj> &mcguffin)
	{
		msgqueue_auto msgqueue(this);
		threadmsgdispatcher_mcguffin=nullptr;

		while (1)
		{
			msgqueue.event();
		}
	}
};


static void test1()
{
	STASHER_NAMESPACE::stoppableThreadTrackerImpl stti(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create());

	STASHER_NAMESPACE::stoppableThreadTracker stt(stti->getTracker());

	x::ref<dummyThreadObj> dt(x::ref<dummyThreadObj>::create());

	x::ptr<x::obj> mcguffin=x::ptr<x::obj>::create();
	x::weakptr<x::ptr<x::obj> > wmcguffin;

	wmcguffin=mcguffin;

	stt->start_threadmsgdispatcher(dt, mcguffin); // [PASSTHRU]

	mcguffin=nullptr;

	if (wmcguffin.getptr().null()) // [START]
	{
		std::cerr << "Thread is not running?" << std::endl;
		throw EXCEPTION("WTF");
	}

	stti->stop_threads(); // [STOPPING]

	if (!wmcguffin.getptr().null())
		throw EXCEPTION("[STOPPING] failed");

	dt=x::ref<dummyThreadObj>::create();
	mcguffin=x::ptr<x::obj>::create();
	wmcguffin=mcguffin;

	stt->start_threadmsgdispatcher(dt, mcguffin); // [ONESHOT]

	mcguffin=nullptr;

	if (!wmcguffin.getptr().null())
	{
		std::cerr << "Thread running after stop_threads()?"
			  << std::endl;
		throw EXCEPTION("WTF");
	}

	mcguffin=x::ptr<x::obj>::create();
	wmcguffin=mcguffin;

	{
		STASHER_NAMESPACE::stoppableThreadTrackerImpl
			stti2(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create());
		stti2->start_threadmsgdispatcher(dt, mcguffin);
		mcguffin=nullptr;

		if (wmcguffin.getptr().null())
		{
			std::cerr << "Thread is not running?" << std::endl;
			throw EXCEPTION("WTF");
		}

		// [DESTRUCTOR]
	}

	if (!wmcguffin.getptr().null())
		throw EXCEPTION("[DESTRUCTOR] failed");
}

static void test2()
{
	STASHER_NAMESPACE::stoppableThreadTrackerImplptr stti(STASHER_NAMESPACE::stoppableThreadTrackerImplptr::create());

	STASHER_NAMESPACE::stoppableThreadTracker stt(stti->getTracker());

	stti=STASHER_NAMESPACE::stoppableThreadTrackerImplptr();

	x::ref<dummyThreadObj> dt(x::ptr<dummyThreadObj>::create());

	x::ptr<x::obj> mcguffin=x::ptr<x::obj>::create();
	x::weakptr<x::ptr<x::obj> > wmcguffin;

	stt->start_threadmsgdispatcher(dt, mcguffin); // [PASSTHRU]
	mcguffin=nullptr;

	if (!wmcguffin.getptr().null()) // [START]
	{
		std::cerr << "Thread is running?" << std::endl;
		throw EXCEPTION("WTF");
	}
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	ALARM(30);

	try {
		test1();
		test2();
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"

#include "fdobjrwthread.H"
#include "writtenobj.H"
#include <x/threads/run.H>
#include <x/destroycallbackflag.H>
#include <x/options.H>

class testrwthreadObj;

class testmsg {

public:
	std::string s;

	testmsg() {}

	testmsg(testrwthreadObj &) {}

	template<typename iter_type> void serialize(iter_type &iter)

	{
		iter(s);
	}
};


class testrwthreadObj : public STASHER_NAMESPACE::fdobjrwthreadObj {

	static const char name[];

public:

	template<typename iter_type>
	static void classlist(iter_type &iter)
	{
		iter.template serialize<testmsg>();
	}

	class makewriteabortObj : public STASHER_NAMESPACE::writtenobjbaseObj {

	public:
		makewriteabortObj() {}
		~makewriteabortObj() noexcept {}

		void serialize(STASHER_NAMESPACE::objwriterObj &writer)
		{
			throw EXCEPTION("Aborted, as per request (makewriteabort)");
		}
	};

	void deserialized(const testmsg &msg)
	{
		std::cerr << "Deserialized " << msg.s << std::endl;

		if (msg.s == "abort")
			throw EXCEPTION("Aborted, as per request");

		typedef STASHER_NAMESPACE::writtenObj<testmsg>::ref_t testmsg_reply;

		testmsg_reply reply(testmsg_reply::create());

		reply->msg.s = "reply:" + msg.s;

		writer->write(reply);
	}

	// [DISPATCH]

	void dispatch(const testmsg &msg)
	{
		std::cerr << "Received " << msg.s << std::endl;

		if (msg.s == "abort")
			throw EXCEPTION("Aborted, as per request");

		if (msg.s == "abortwrite")
		{
			writer->write(x::ptr<makewriteabortObj>::create());
			return;
		}

		typedef STASHER_NAMESPACE::writtenObj<testmsg>::ref_t testmsg_reply;

		testmsg_reply reply(testmsg_reply::create());

		reply->msg.s = "dispatch:" + msg.s;

		writer->write(reply);
	}

	testrwthreadObj()
	: STASHER_NAMESPACE::fdobjrwthreadObj(std::string(name) + " (writer)")
	{
	}

	~testrwthreadObj() noexcept
	{
	}

	void run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
		 const x::fdbase &transport,
		 const x::fd::base::inputiter &inputiterArg,
		 const STASHER_NAMESPACE::stoppableThreadTracker &tracker,
		 const x::ptr<x::obj> &mcguffin)
	{
		msgqueue_auto msgqueue(this);
		threadmsgdispatcher_mcguffin=x::ptr<x::obj>();

		mainloop(msgqueue, transport, inputiterArg, tracker, mcguffin);
	}

	template<typename ...Args>
	void submit(Args && ...args)
	{
		x::threadmsgdispatcherObj::sendevent(&testrwthreadObj::dispatch,
						     this,
						     std::forward<Args>(args)...
						     );
	}

	MAINLOOP_DECL;
};

MAINLOOP_IMPL(testrwthreadObj)

typedef x::ref<testrwthreadObj> testrwthread;

typedef x::ptr<testrwthreadObj> testrwthreadptr;

const char testrwthreadObj::name[]="testthread";

void wait_mcguffin(x::ptr<x::obj> &mcguffin) // [RWMCGUFFIN]
{
	x::destroyCallbackFlag cb(x::destroyCallbackFlag::create());

	mcguffin->ondestroy([cb]{cb->destroyed();});

	mcguffin=x::ptr<x::obj>();

	std::cerr << "Waiting for all threads to stop (1 of 2)" << std::endl;

	cb->wait();

	std::cerr << "Waiting for all threads to stop (2 of 2)" << std::endl;
}

class testsuite {

public:
	STASHER_NAMESPACE::stoppableThreadTrackerImplptr tracker;

	testrwthreadptr thr;

	x::fdptr sockto;

	x::ptr<x::obj> mcguffin;

	void start()
	{
		std::pair<x::fd, x::fd> socks(x::fd::base::socketpair());

		socks.second->nonblock(true);

		sockto=socks.first;

		tracker=STASHER_NAMESPACE::stoppableThreadTrackerImpl::create();

		testrwthread p=testrwthread::create();

		thr=p;

		mcguffin=x::ptr<x::obj>::create();

		tracker->start_thread(p, socks.second,
				      x::fd::base::inputiter(socks.second),
				      tracker->getTracker(), mcguffin);
	}
};

void test1()
{
	testsuite test;

	test.start();

	test.thr->stop(); // [RWSTOP]

	wait_mcguffin(test.mcguffin);
}

void test2()
{
	testsuite test;

	test.start();

	test.sockto->close(); // [CLOSED]

	wait_mcguffin(test.mcguffin);
}

void test3()
{
	testsuite test;

	test.start();

	x::fd::base::inputiter initer(test.sockto), inenditer;

	// [DESERIALIZATION]
	{
		testmsg msg;

		msg.s="foo";

		x::fd::base::outputiter outiter(test.sockto);

		x::serialize::iterator<x::fd::base::outputiter>
			ser_iter(outiter);

		ser_iter(msg);
		outiter.flush();
	}

	{
		testmsg msg;

		x::deserialize::iterator<x::fd::base::inputiter>
			deser_iter(initer, inenditer);

		deser_iter(msg);

		if (msg.s != "reply:foo")
			throw EXCEPTION("Unexpected reply to foo: " + msg.s);
	}
}

void test4()
{
	testsuite test;

	test.start();

	x::fd::base::inputiter initer(test.sockto), inenditer;

	// [DISPATCH]
	{
		testmsg msg;

		msg.s="bar";

		test.thr->submit(msg);
	}

	{
		testmsg msg;

		x::deserialize::iterator<x::fd::base::inputiter>
			deser_iter(initer, inenditer);

		deser_iter(msg);

		if (msg.s != "dispatch:bar")
			throw EXCEPTION("Unexpected reply to bar: " + msg.s);
	}
}

void test5()
{
	testsuite test;

	test.start();

	x::fd::base::inputiter initer(test.sockto), inenditer;

	// [MAINABORTS]
	{
		testmsg msg;

		msg.s="abort";

		x::fd::base::outputiter outiter(test.sockto);

		x::serialize::iterator<x::fd::base::outputiter>
			ser_iter(outiter);

		ser_iter(msg);
		outiter.flush();
	}
	wait_mcguffin(test.mcguffin);
}

void test6()
{
	testsuite test;

	test.start();

	x::fd::base::inputiter initer(test.sockto), inenditer;

	// [MAINABORTS]
	{
		testmsg msg;

		msg.s="abort";

		test.thr->submit(msg);
	}

	wait_mcguffin(test.mcguffin);
}

void test7()
{
	testsuite test;

	test.start();

	x::fd::base::inputiter initer(test.sockto), inenditer;

	// [WRITERABORTS]
	{
		testmsg msg;

		msg.s="abortwrite";

		test.thr->submit(msg);
	}

	wait_mcguffin(test.mcguffin);
}

void test8() // [TRACKER]
{
	testsuite test;

	test.start();

	test.tracker->stop_threads();
	wait_mcguffin(test.mcguffin);
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	ALARM(30);

	try {
		std::cerr << "test1" << std::endl;
		test1();
		std::cerr << "test2" << std::endl;
		test2();
		std::cerr << "test3" << std::endl;
		test3();
		std::cerr << "test4" << std::endl;
		test4();
		std::cerr << "test5" << std::endl;
		test5();
		std::cerr << "test6" << std::endl;
		test6();
		std::cerr << "test7" << std::endl;
		test7();
		std::cerr << "test8" << std::endl;
		test8();
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

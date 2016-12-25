/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"

#include "fdobjreaderthread.H"
#include "threadmgr.H"
#include <x/threads/run.H>
#include <x/options.H>
#include <x/sysexception.H>

class dummymsg {

public:

	std::string str;

	dummymsg() {}
	template<typename x> dummymsg(x &dummy) {}

	template<typename iter_type> void serialize(iter_type &iter)

	{
		iter(str);
	}
};

// [CLASS]

static size_t counter=0;

class dummyhandlerObj : public STASHER_NAMESPACE::fdobjreaderthreadObj {

public:

	template<typename ser_type> static void classlist(ser_type &arg)

	{
		arg.template serialize<dummymsg>();
	}


	std::vector<std::string> desered;

	std::vector<std::string> received;

	std::mutex mutex;
	std::condition_variable cond;

	dummyhandlerObj()
	{
		++counter;
	}

	~dummyhandlerObj() noexcept {--counter; }

	template<typename ...Args>
	void event(Args && ...args)
	{
		x::threadmsgdispatcherObj::sendevent(&dummyhandlerObj::dispatch,
						     this,
						     std::forward<Args>(args)...
						     );
	}

	void deserialized(const dummymsg &msg)
	{
		desered.push_back(msg.str);
	}

	void dispatch(const dummymsg &msg)
	{
		std::lock_guard<std::mutex> lock(mutex);

		received.push_back(msg.str);
		cond.notify_all();

	}

	size_t pubread(const x::fdbase &transport,
		       char *buffer,
		       size_t cnt)
	{
		size_t n;

		do
		{
			drain();

			struct pollfd pfd[2];

			pfd[0].fd=transport->getFd();
			pfd[1].fd=get_msgqueue()->getEventfd()->getFd();

			pfd[0].events=POLLIN;
			pfd[1].events=POLLIN;

			int rc=poll(pfd, 2, -1);

			if (rc < 0)
				throw SYSEXCEPTION("poll");

			if (rc == 0)
				continue;
		} while ((n=transport->pubread(buffer, cnt)) == 0
			 && (errno == EAGAIN || errno == EWOULDBLOCK));

		return n;
	}

	void run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
		 start_thread_sync &sync_arg,
		 const x::fd &fd)
	{
		msgqueue_auto msgqueue(this);
		threadmsgdispatcher_mcguffin=nullptr;
		sync_arg->thread_started();

		x::fd::base::inputiter beg_iter(fd);

		mainloop(msgqueue, fd, beg_iter);
	}

	MAINLOOP_DECL;
};

MAINLOOP_IMPL(dummyhandlerObj)

typedef x::ptr<dummyhandlerObj> dummyhandler;

typedef x::ptr<STASHER_NAMESPACE::fdobjreaderthreadObj> reader;

static void test1()
{
	std::pair<x::fd, x::fd> socks(x::fd::base::socketpair());

	socks.second->nonblock(true);

	dummyhandler h(dummyhandler::create());

	auto howner=x::ref<threadmgrObj<dummyhandler> >::create();

	howner->start_thread(h, socks.second);

	{
		dummymsg msg;

		msg.str="foo";

		howner->event(msg); // [MESSAGE]
	}

	std::unique_lock<std::mutex> lock(h->mutex);

	while (h->received.empty())
		h->cond.wait(lock);

	howner->stop(); // [STOP]
	howner->wait();
}

static void test2()
{
	std::pair<x::fd, x::fd> socks(x::fd::base::socketpair());

	socks.second->nonblock(true);

	dummyhandler h(dummyhandler::create());

	auto howner=x::ref<threadmgrObj<dummyhandler> >::create();

	howner->start_thread(h, socks.second);

	{
		dummymsg msg;

		msg.str="foo";

		howner->event(msg);
	}

	std::unique_lock<std::mutex> lock(h->mutex);

	while (h->received.empty())
		h->cond.wait(lock);

	socks.first->close(); // [STOP]
	howner->wait();
}

static void test3()
{
	std::pair<x::fd, x::fd> socks(x::fd::base::socketpair());

	socks.second->nonblock(true);

	dummyhandler h(dummyhandler::create());

	auto howner=x::ref<threadmgrObj<dummyhandler> >::create();

	howner->start_thread(h, socks.second);

	try {
		howner->start_thread(h, socks.second);
	} catch (const x::exception &e)
	{
		std::cerr << "Expected error: " << e << std::endl;
		return; // [STOP]
	}

	throw EXCEPTION("An exception was not thrown, for some reason");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	ALARM(30);

	try {
		std::cout << "test1" << std::endl;
		test1();
		std::cout << "test2" << std::endl;
		test2();
		std::cout << "test3" << std::endl;
		test3();

		if (counter != 0)
			throw EXCEPTION("Reference count is not 0");

	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

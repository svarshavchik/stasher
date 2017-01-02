/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"

#include "objwriterthread.H"

#include <x/deserialize.H>
#include <x/weakptr.H>
#include <x/options.H>
#include <x/threads/run.H>
#include <vector>

class test : public STASHER_NAMESPACE::objwriterthreadObj {

public:

	using STASHER_NAMESPACE::objwriterthreadObj::run;

	bool goahead;

	std::mutex mutex;
	std::condition_variable cond;

	test() : STASHER_NAMESPACE::objwriterthreadObj("test", 256),
		 goahead(false)
	{
	}

	~test()
	{
	}

	void run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
		 const x::ref<x::obj> &mcguffin)
	{
		msgqueue_auto msgqueue(this);

		threadmsgdispatcher_mcguffin=nullptr;
		STASHER_NAMESPACE::objwriterthreadObj::run(msgqueue, mcguffin);
	}

	void foo() override {}

	std::vector<char> buffer;

	// [SUBCLASS]
	size_t flush(const char *ptr, size_t cnt)
	{
		std::unique_lock<std::mutex> lock(mutex);

		while (!goahead)
			cond.wait(lock);

		if (cnt > 128)
			cnt=128;

		buffer.insert(buffer.end(), ptr, ptr+cnt);
		return cnt;
	}
};

class dummy : public STASHER_NAMESPACE::writtenobjbaseObj {

public:

	std::vector<char> vec;

	dummy() noexcept
	{
		for (size_t i=0; i<500; ++i)
			vec.push_back(i);
	}

	~dummy()
	{
	}

	void serialize(STASHER_NAMESPACE::objwriterObj &writer)
	{
		writer.serialize(vec);
	}
};

static void test1()
{
	auto t=x::ref<test>::create();
	auto d=x::ref<dummy>::create();

	auto retval=x::start_threadmsgdispatcher(t, x::ref<x::obj>::create());

	t->write(d); // [WRITE]
	t->request_close(); // [CLOSE]

        {
                std::unique_lock<std::mutex> lock(t->mutex);

                t->goahead=true;

                t->cond.notify_all();
        }

	retval->wait();

	std::vector<char>::iterator b(t->buffer.begin()), e(t->buffer.end());

	x::deserialize::iterator<std::vector<char>::iterator>
		deser_iter(b, e);

	std::vector<char> dummy2;

	deser_iter(dummy2);

	if (d->vec != dummy2)
		throw EXCEPTION("Round trip failed");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	ALARM(30);
	try {
		test1();
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

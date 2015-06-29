/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"

#include "fdobjwriterthread.H"
#include "writtenobjbase.H"
#include "stoppablethreadtracker.H"
#include <x/deserialize.H>
#include <x/options.H>
#include <x/fditer.H>
#include <x/deserialize.H>
#include <vector>
#include <signal.h>

class dummy : public STASHER_NAMESPACE::writtenobjbaseObj {

public:

	std::vector<char> vec;

	dummy() noexcept
	{
		for (size_t i=0; i<500; ++i)
			vec.push_back(i);
	}

	~dummy() noexcept
	{
	}

	void serialize(STASHER_NAMESPACE::objwriterObj &writer)
	{
		writer.serialize(vec);
	}
};

static std::pair<STASHER_NAMESPACE::stoppableThreadTrackerImpl,
		 x::runthread<void> >
runw(const x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj> &thr,
     const x::fdbase &fd, const x::ref<x::obj> &mcguffin=x::ref<x::obj>::create())
{
	auto tracker=STASHER_NAMESPACE::stoppableThreadTrackerImpl::create();

	return std::make_pair(tracker, tracker->start(thr, fd, mcguffin));
}

static void test1()
{
	std::pair<x::fd, x::fd> socks(x::fd::base::socketpair());

	socks.first->nonblock(true);
	socks.second->nonblock(true);

	auto writer=x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj>
		::create("test1");

	auto thread=runw(writer, socks.first);
	// [WRITESTART]

	socks.second->close();
	writer->stop();
	thread.second->wait();
}

static void test2()
{
	std::pair<x::fd, x::fd> socks(x::fd::base::socketpair());

	socks.first->nonblock(true);
	socks.second->nonblock(true);

	auto writer=x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj>
		::create("test2");

	auto thread=runw(writer, socks.first);

	socks.second->close(); // [ERROR]

	writer->write(x::ptr<dummy>::create());
	thread.second->get();
}

static void test3()
{
	x::fd tmpfile(x::fd::base::tmpfile());

	tmpfile->nonblock(true);

	auto writer=
		x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj>
		::create("test3");

	auto thread=runw(writer, tmpfile);

	x::ptr<dummy> obj(x::ptr<dummy>::create());

	writer->write(obj);
	writer->request_close();

	thread.second->get();

	tmpfile->seek(0, SEEK_SET);

	{
		x::istream i(tmpfile->getistream());

		x::streambuf ibuf(i->rdbuf());

		std::istreambuf_iterator<char> iter(&*ibuf), iter_end;

		x::deserialize
			::iterator< std::istreambuf_iterator<char> >
			ser_iter(iter, iter_end);

		std::vector<char> vec;

		ser_iter(vec);

		if (vec != obj->vec)
			throw EXCEPTION("Deserialization mismatch");
	}
}

static void test4()
{
	std::pair<x::fd, x::fd> socks(x::fd::base::socketpair());

	socks.first->nonblock(true);
	socks.second->nonblock(true);

	auto writer=x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj>
		::create("test4");

	auto thread=runw(writer, socks.first);

	writer->request_close();
	thread.second->wait();

	// [RECYCLE]

	socks=x::fd::base::socketpair();
	writer=x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj>
		::create("test4");

	socks.first->nonblock(true);
	thread=runw(writer, socks.first);
	writer->write(x::ref<dummy>::create());

	{
		char buf;

		socks.second->read(&buf, 1);
	}

	auto thread2=runw(writer, socks.first);

	bool caught=false;

	try {
		thread2.second->get();
	} catch (...) {
		caught=true;
	}

	if (!caught)
		throw EXCEPTION("[RECYCLE] failed");
}

class bigdummy : public STASHER_NAMESPACE::writtenobjbaseObj {

public:

	std::vector<char> vec;

	bigdummy() noexcept
	{
		for (size_t i=0; i<500; ++i)
			vec.push_back(i);
	}

	~bigdummy() noexcept
	{
	}

	void serialize(STASHER_NAMESPACE::objwriterObj &writer)
	{
		while (1)
			writer.serialize(vec);
	}
};

static void test5()
{
	static x::property::value<unsigned> testprop("testprop1", 0);

	std::pair<x::fd, x::fd> socks(x::fd::base::socketpair());

	socks.first->nonblock(true);
	socks.second->nonblock(true);

	auto writer=x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj>
		::create("test5", &testprop);

	auto thread=runw(writer, socks.first);
	writer->write(x::ptr<bigdummy>::create()); // [WRITETIMEOUT]

	thread.second->wait();
}

static void test6()
{
	std::pair<x::fd, x::fd> socks(x::fd::base::socketpair());

	socks.first->nonblock(true);

	auto writer=x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj>
		::create("test6");

	auto thread=runw(writer, socks.first);
	writer->write(x::ref<bigdummy>::create());

	char dummy[1024];

	socks.second->read(dummy, sizeof(dummy));
	writer->stop();
	thread.second->wait();
}

static void test7()
{
	std::pair<x::fdptr, x::fdptr> socks(x::fd::base::socketpair());

	socks.first->nonblock(true);

	auto writer=
		x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj>
		::create("test7");

	auto thread=runw(writer, socks.first);
	socks.first=x::fdptr();

	x::fdptr tmpfile=x::fd::base::tmpfile();

	(*tmpfile->getostream()) << "foo" << std::endl << std::flush;

	tmpfile->seek(0, SEEK_SET);

	writer->write(x::ref<dummy>::create());
	{
		std::vector<x::fd> fd;

		fd.push_back(tmpfile);

		writer->sendfd(fd);
	}
	tmpfile=x::fdptr();
	writer->write(x::ref<dummy>::create());

	x::fd::base::inputiter beg_iter(socks.second), end_iter;

	x::deserialize::iterator<x::fd::base::inputiter>
		deser_iter(beg_iter, end_iter);

	{
		std::vector<char> buffer;

		deser_iter(buffer);
	}

	{
		STASHER_NAMESPACE::fdobjwriterthreadObj::sendfd_coming msg;

		deser_iter(msg);

		STASHER_NAMESPACE::fdobjwriterthreadObj::sendfd_ready ack;

		writer->sendfd_proceed(ack);
	}

	socks.second->recv_fd(&tmpfile, 1);

	{
		std::vector<char> buffer;

		deser_iter(buffer);
	}

	std::string s;
	std::getline(*tmpfile->getistream(), s);

	if (s != "foo") // [SENDFD]
		throw EXCEPTION("[SENDFD] failed");
}

static void test8()
{
	std::pair<x::fdptr, x::fdptr> socks(x::fd::base::socketpair());

	socks.first->nonblock(true);

	auto writer=
		x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj>
		::create("test8");

	auto thread=runw(writer, socks.first);
	socks.first=x::fdptr();
	writer->setmaxqueuesize(10);

	// [MAXWRITEQUEUESIZE]
	for (size_t i=0; i<20; ++i)
		writer->write(x::ref<bigdummy>::create());
	thread.second->wait();
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	std::cout << "test1" << std::endl;
	test1();
	std::cout << "test2" << std::endl;
	test2();
	std::cout << "test3" << std::endl;
	test3();
	std::cout << "test4" << std::endl;
	test4();
	std::cout << "test5" << std::endl;
	test5();
	std::cout << "test6" << std::endl;
	test6();
	std::cout << "test7" << std::endl;
	test7();
	std::cout << "test8" << std::endl;
	test8();

	return 0;
}

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "stasher/userinit.H"
#include "stasher/userput.H"
#include "userput_deserialized.H"
#include "stasher/puttransaction.H"
#include "tobjrepo.H"
#include "newtran.H"
#include "trancommit.H"
#include "fdobjwriterthread.H"
#include "stoppablethreadtracker.H"
#include "tst.nodes.H"
#include "node.H"
#include "stasher/client.H"
#include "deserobjname.H"
#include "nsmap.H"
#include <x/options.H>
#include <x/fditer.H>
#include <x/deserialize.H>
#include <fstream>

class up_test {

public:
	STASHER_NAMESPACE::stoppableThreadTrackerImpl tracker;
	x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj> writer;
	x::fdptr socket;
	tobjrepo repo;
	spacemonitor monitor;

	up_test()
	: tracker(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create()),
	  writer(x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj>
		 ::create("test")),
	  repo(tobjrepo::create("conftestdir.tst")),
	  monitor(spacemonitor::create(x::df::create("conftestdir.tst")))
	{
		std::pair<x::fd, x::fd> socks=x::fd::base::socketpair();

		socket=socks.first;

		socks.second->nonblock(true);

		tracker->start(writer, socks.second, x::ref<x::obj>::create());
	}

	void verify(const std::string &objname,
		    const std::string &objvalue)
	{
		tobjrepoObj::values_t valuesMap;

		std::set<std::string> notfound;
		std::set<std::string> s;

		s.insert(objname);

		repo->values(s, true, valuesMap, notfound);

		std::string str;

		std::getline(*valuesMap[objname].second->getistream(), str);

		if (str != objvalue)
			throw EXCEPTION("[USERPUTDESER] failed");
	}

};

void test1()
{
	up_test upt;

	STASHER_NAMESPACE::userinit limits(10, 100, 1000000, 8192, 10);

	{
		x::ref<STASHER_NAMESPACE::puttransactionObj>
			pt=x::ref<STASHER_NAMESPACE::puttransactionObj>::create();

		pt->newobj("obj1", "foo\n");

		x::fd fd=x::fd::base::tmpfile();

		(*fd->getostream()) << "bar" << std::endl << std::flush;

		pt->newobj("obj2", fd);

		STASHER_NAMESPACE::userput(pt, limits).write(upt.writer); // [USERPUTWRITE]
	}

	x::fd::base::inputiter beg_iter(upt.socket), end_iter;

	x::deserialize::iterator<x::fd::base::inputiter> iter(beg_iter, end_iter);

	newtran tr=({
			auto deser=
				x::ref<STASHER_NAMESPACE::userput::deserializedObj> 
				::create((
					{
						STASHER_NAMESPACE::userput up(limits);

						iter(up);

						up.tran;
					}), limits, upt.monitor, upt.repo,
					x::uuid(), nsview::create());


			{
				STASHER_NAMESPACE::puttransactionObj::content_str str(8192);

				iter(str);

				deser->received(str);
			}

			{
				STASHER_NAMESPACE::fdobjwriterthreadObj::sendfd_coming msg;

				iter(msg);

				upt.writer->sendfd_proceed(STASHER_NAMESPACE::fdobjwriterthreadObj
							   ::sendfd_ready());
			}

			{
				x::fdptr fd;

				upt.socket->recv_fd(&fd, 1);

				deser->received(fd);
			}

			if (!deser->completed())
				throw EXCEPTION("test1 failed");
			deser->tran;
		});

	upt.repo->begin_commit(tr->finalize(), x::eventfd::create())->commit();

	upt.verify("obj1", "foo");
	upt.verify("obj2", "bar");
}

void test2()
{
	up_test upt;

	STASHER_NAMESPACE::userinit limits(10, 100, 1000000, 2, 10);

	{
		x::ref<STASHER_NAMESPACE::puttransactionObj>
			pt=x::ref<STASHER_NAMESPACE::puttransactionObj>::create();

		pt->newobj("obj3", "");
		pt->newobj("obj4", "foo\n");
		STASHER_NAMESPACE::userput(pt, limits).write(upt.writer);
	}

	x::fd::base::inputiter beg_iter(upt.socket), end_iter;

	x::deserialize::iterator<x::fd::base::inputiter> iter(beg_iter, end_iter);

	newtran tr=({
			auto deser=
				x::ref<STASHER_NAMESPACE::userput::deserializedObj>
				::create
				(({
						STASHER_NAMESPACE::userput up(limits);

						iter(up);

						up.tran;
					}), limits, upt.monitor, upt.repo,
					x::uuid(), nsview::create());


			{
				STASHER_NAMESPACE::puttransactionObj::content_str str(8192);

				iter(str);

				deser->received(str);

				if (str.str != "")
					throw EXCEPTION("[USERPUTSPLIT] failed (1)");
			}

			{
				STASHER_NAMESPACE::puttransactionObj::content_str str(8192);

				iter(str);

				deser->received(str);

				if (str.str != "fo")
					throw EXCEPTION("[USERPUTSPLIT] failed (1)");
			}

			{
				STASHER_NAMESPACE::puttransactionObj::content_str str(8192);

				iter(str);

				deser->received(str);

				if (str.str != "o\n")
					throw EXCEPTION("[USERPUTSPLIT] failed (1)");
			}

			if (!deser->completed())
				throw EXCEPTION("test1 failed");
			deser->tran;
		});

	upt.repo->begin_commit(tr->finalize(), x::eventfd::create())->commit();

	upt.verify("obj3", "");
	upt.verify("obj4", "foo");
}

void test3()
{
	up_test upt;

	STASHER_NAMESPACE::userinit limits(10, 100, 1000000, 8192, 10);

	{
		x::ref<STASHER_NAMESPACE::puttransactionObj>
			pt=x::ref<STASHER_NAMESPACE::puttransactionObj>::create();

		pt->newobj("obj3", "");
		pt->newobj("obj4", "foo\n");
		STASHER_NAMESPACE::userput(pt, limits).write(upt.writer);
	}

	x::fd::base::inputiter beg_iter(upt.socket), end_iter;

	x::deserialize::iterator<x::fd::base::inputiter> iter(beg_iter, end_iter);

	try {
		STASHER_NAMESPACE::userput up(STASHER_NAMESPACE::userinit(10, 1, 1000000, 8192, 10));
		iter(up);
	} catch (const x::exception &e)
	{
		std::cerr << "Expected exception: " << e << std::endl;
		return;
	}

	throw EXCEPTION("Expected deserialization failure did not happen");
}

void test4()
{
	up_test upt;

	STASHER_NAMESPACE::userinit limits(10, 100, 1000000, 2, 10);

	{
		x::ref<STASHER_NAMESPACE::puttransactionObj>
			pt=x::ref<STASHER_NAMESPACE::puttransactionObj>::create();

		pt->newobj("obj3", "");
		pt->newobj("obj4", "foo\n");
		STASHER_NAMESPACE::userput(pt, limits).write(upt.writer);
	}

	x::fd::base::inputiter beg_iter(upt.socket), end_iter;

	x::deserialize::iterator<x::fd::base::inputiter> iter(beg_iter, end_iter);

	try {
		STASHER_NAMESPACE::userinit deser_limits(10, 100, 2, 8192, 10);

		auto deser=
			x::ref<STASHER_NAMESPACE::userput::deserializedObj>
			::create
			(({
					STASHER_NAMESPACE::userput up(deser_limits);

					iter(up);

					up.tran;
				}), deser_limits, upt.monitor,
				upt.repo, x::uuid(), nsview::create());

	} catch (const x::exception &e)
	{
		std::cerr << "Expected exception: " << e << std::endl;
		return;
	}

	throw EXCEPTION("Expected deserialization failure did not happen");
}

void test5()
{
	up_test upt;

	STASHER_NAMESPACE::userinit limits(10, 100, 1000000, 2, 10);

	{
		x::ptr<STASHER_NAMESPACE::puttransactionObj>
			pt=x::ptr<STASHER_NAMESPACE::puttransactionObj>::create();

		x::fd fd=x::fd::base::tmpfile();

		(*fd->getostream()) << "foobar\n" << std::flush;
		pt->newobj("obj4", fd);
		STASHER_NAMESPACE::userput(pt, limits).write(upt.writer);
	}

	x::fd::base::inputiter beg_iter(upt.socket), end_iter;

	x::deserialize::iterator<x::fd::base::inputiter> iter(beg_iter, end_iter);

	try {
		STASHER_NAMESPACE::userinit deser_limits(10, 100, 2, 8192, 10);

		auto deser=
			x::ref<STASHER_NAMESPACE::userput::deserializedObj>
			::create
			(({
					STASHER_NAMESPACE::userput up(deser_limits);

					iter(up);

					up.tran;
				}), deser_limits, upt.monitor,
				upt.repo, x::uuid(), nsview::create());

	} catch (const x::exception &e)
	{
		std::cerr << "Expected exception: " << e << std::endl;
		return;
	}

	throw EXCEPTION("Expected deserialization failure did not happen");
}

void test6()
{
	up_test upt;

	STASHER_NAMESPACE::userinit limits(10, 100, 1000000, 2, 10);

	{
		x::ptr<STASHER_NAMESPACE::puttransactionObj>
			pt=x::ptr<STASHER_NAMESPACE::puttransactionObj>::create();

		x::fd fd=x::fd::base::tmpfile();

		pt->newobj("obj4", fd);
		STASHER_NAMESPACE::userput(pt, limits).write(upt.writer);
		(*fd->getostream()) << "foobar\n" << std::flush;
	}

	x::fd::base::inputiter beg_iter(upt.socket), end_iter;

	x::deserialize::iterator<x::fd::base::inputiter> iter(beg_iter, end_iter);

	try {
		STASHER_NAMESPACE::userinit deser_limits(10, 100, 2, 8192, 10);

		auto deser=x::ref<STASHER_NAMESPACE::userput::deserializedObj>
			::create
			(({
					STASHER_NAMESPACE::userput up(deser_limits);

					iter(up);

					up.tran;
				}), deser_limits, upt.monitor,
				upt.repo, x::uuid(), nsview::create());
		{
			STASHER_NAMESPACE::fdobjwriterthreadObj::sendfd_coming msg;

			iter(msg);

			upt.writer->sendfd_proceed(STASHER_NAMESPACE::fdobjwriterthreadObj
						   ::sendfd_ready());
		}

		{
			x::fdptr fd;

			upt.socket->recv_fd(&fd, 1);

			deser->received(fd);
		}

	} catch (const x::exception &e)
	{
		std::cerr << "Expected exception: " << e << std::endl;
		return;
	}

	throw EXCEPTION("Expected deserialization failure did not happen");
}

void test7()
{
	up_test upt;

	STASHER_NAMESPACE::userinit limits(10, 100, 1000000, 2, 10);

	{
		x::ptr<STASHER_NAMESPACE::puttransactionObj>
			pt=x::ptr<STASHER_NAMESPACE::puttransactionObj>::create();

		x::fd fd=x::fd::base::open("/dev/null", O_RDONLY);

		pt->newobj("obj4", fd);
		STASHER_NAMESPACE::userput(pt, limits).write(upt.writer);
	}

	x::fd::base::inputiter beg_iter(upt.socket), end_iter;

	x::deserialize::iterator<x::fd::base::inputiter> iter(beg_iter, end_iter);

	try {
		STASHER_NAMESPACE::userinit deser_limits(10, 100, 2, 8192, 10);

		auto deser=x::ref<STASHER_NAMESPACE::userput::deserializedObj>
			::create
			(({
					STASHER_NAMESPACE::userput up(deser_limits);

					iter(up);

					up.tran;
				}), deser_limits, upt.monitor,
				upt.repo, x::uuid(), nsview::create());

		{
			STASHER_NAMESPACE::fdobjwriterthreadObj::sendfd_coming msg;

			iter(msg);

			upt.writer->sendfd_proceed(STASHER_NAMESPACE::fdobjwriterthreadObj
						   ::sendfd_ready());
		}

		{
			x::fdptr fd;

			upt.socket->recv_fd(&fd, 1);

			deser->received(fd);
		}

	} catch (const x::exception &e)
	{
		std::cerr << "Expected exception: " << e << std::endl;
		return;
	}

	throw EXCEPTION("Expected deserialization failure did not happen");
}

void test8(off_t fake)
{
	up_test upt;

	STASHER_NAMESPACE::userinit limits(10, 100, 1000000, 8192, 10);

	{
		x::ptr<STASHER_NAMESPACE::puttransactionObj>
			pt=x::ptr<STASHER_NAMESPACE::puttransactionObj>::create();

		pt->newobj("obj4", "foobar\n");

		pt->objects[0].contents_size=fake;
		STASHER_NAMESPACE::userput(pt, limits).write(upt.writer);
	}

	x::fd::base::inputiter beg_iter(upt.socket), end_iter;

	x::deserialize::iterator<x::fd::base::inputiter> iter(beg_iter, end_iter);

	try {
		STASHER_NAMESPACE::userinit
			deser_limits(10, 100, 1000000, 8192, 10);

		auto deser=x::ref<STASHER_NAMESPACE::userput::deserializedObj>
			::create
			(({
					STASHER_NAMESPACE::userput up(deser_limits);

					iter(up);

					up.tran;
				}), deser_limits, upt.monitor,
				upt.repo, x::uuid(), nsview::create());

		{
			STASHER_NAMESPACE::puttransactionObj::content_str str(8192);

			iter(str);

			deser->received(str);

		}

	} catch (const x::exception &e)
	{
		std::cerr << "Expected exception: " << e << std::endl;
		return;
	}

	throw EXCEPTION("Expected deserialization failure did not happen");
}

void test9()
{
	up_test upt;

	STASHER_NAMESPACE::userinit limits(10, 100, 1000000, 8192, 10);

	{
		x::ptr<STASHER_NAMESPACE::puttransactionObj>
			pt=x::ptr<STASHER_NAMESPACE::puttransactionObj>::create();

		pt->newobj("obj4", "foobar\n");
		STASHER_NAMESPACE::userput(pt, limits).write(upt.writer);
	}

	x::fd::base::inputiter beg_iter(upt.socket), end_iter;

	x::deserialize::iterator<x::fd::base::inputiter> iter(beg_iter, end_iter);

	try {
		STASHER_NAMESPACE::userinit
			deser_limits(10, 100, 1000000, 1, 10);

		auto deser=x::ptr<STASHER_NAMESPACE::userput::deserializedObj>
			::create
			(({
					STASHER_NAMESPACE::userput up(deser_limits);

					iter(up);

					up.tran;
				}), deser_limits, upt.monitor,
				upt.repo, x::uuid(), nsview::create());

		{
			STASHER_NAMESPACE::puttransactionObj::content_str str(1);

			iter(str);

			deser->received(str);

		}

	} catch (const x::exception &e)
	{
		std::cerr << "Expected exception: " << e << std::endl;
		return;
	}

	throw EXCEPTION("Expected deserialization failure did not happen");
}

void test9b()
{
	up_test upt;

	STASHER_NAMESPACE::userinit limits(10, 100, 1000000, 8192, 10);

	{
		x::ptr<STASHER_NAMESPACE::puttransactionObj>
			pt=x::ptr<STASHER_NAMESPACE::puttransactionObj>::create();

		pt->newobj("obj4", "foobar\n");
		pt->objects[0].name=
			std::string(objrepoObj::maxnamesize+1, 'x');

		STASHER_NAMESPACE::userput(pt, limits).write(upt.writer);
	}

	x::fd::base::inputiter beg_iter(upt.socket), end_iter;

	x::deserialize::iterator<x::fd::base::inputiter> iter(beg_iter, end_iter);

	try {
		STASHER_NAMESPACE::userinit
			deser_limits(10, 100, 1000000, 1, 10);

		STASHER_NAMESPACE::userput up(deser_limits);

		iter(up);

	} catch (const x::exception &e)
	{
		std::cerr << "Expected exception: " << e << std::endl;
		return;
	}

	throw EXCEPTION("Expected deserialization failure did not happen");
}




static void test10(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	t.startmastercontrolleron0_int(tnodes);

	STASHER_NAMESPACE::client cl=
		STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	STASHER_NAMESPACE::client::base::transaction tran=STASHER_NAMESPACE::client::base::transaction::create();

	// Test accessability
	{
		STASHER_NAMESPACE::client::base::transaction tran2(tran);

		tran2=tran;
	}

	tran->newobj("obj1", "obj1_value\n");
	STASHER_NAMESPACE::putresults res=cl->put(tran);

	if (res->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION(x::tostring(res->status));

	std::cerr << "Transaction success: "
		  << x::tostring(res->newuuid) << std::endl;

	tran=STASHER_NAMESPACE::client::base::transaction::create();

	{
		x::fd fd=x::fd::base::tmpfile();

		(*fd->getostream()) << "obj1_newvalue"
				    << std::endl << std::flush;

		tran->updobj("obj1", res->newuuid, fd);
	}

	res=cl->put(tran);

	if (res->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION(x::tostring(res->status));

	std::cerr << "Transaction success: "
		  << x::tostring(res->newuuid) << std::endl;

	for (size_t i=0; i<2; ++i)
	{
		tobjrepoObj::values_t valuesMap;

		std::set<std::string> notfound;
		std::set<std::string> s;

		s.insert("obj1");

		tnodes[i]->repo->values(s, true, valuesMap, notfound);

		std::string str;

		std::getline(*valuesMap["obj1"].second->getistream(),
			     str);

		if (str != "obj1_newvalue")
			throw EXCEPTION("[CLIENTPUT] failed");
	}

	tran=STASHER_NAMESPACE::client::base::transaction::create();
	tran->delobj("obj1", x::uuid());

	{
		STASHER_NAMESPACE::putresults res2=cl->put(tran);

		if (res2->status == STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION("Transaction did not fail, as expected");

		std::cerr << "Expected error: " << x::tostring(res2->status)
			  << std::endl;
	}

	tran=STASHER_NAMESPACE::client::base::transaction::create();
	tran->delobj("obj1", res->newuuid);

	res=cl->put(tran);

	if (res->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION(x::tostring(res->status));

	tran=STASHER_NAMESPACE::client::base::transaction::create();
	tran->newobj("obj1//.err", x::fd::base::tmpfile());

	{
		STASHER_NAMESPACE::putresults res2=cl->put(tran);

		if (res2->status == STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION("Transaction did not fail, as expected");

		std::cerr << "Expected error: " << x::tostring(res2->status)
			  << std::endl;
	}

	tran=STASHER_NAMESPACE::client::base::transaction::create();
	tran->newobj("obj1", "obj1_value");
	tran->newobj("obj1", "obj1_value");
	{
		STASHER_NAMESPACE::putresults res2=cl->put(tran);

		if (res2->status == STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION("Transaction did not fail, as expected");

		std::cerr << "Expected error: " << x::tostring(res2->status)
			  << std::endl;
	}

	tran=STASHER_NAMESPACE::client::base::transaction::create();
	tran->newobj(std::string(objrepoObj::maxnamesize+1, 'x'),
		     "obj1_value");
	{
		STASHER_NAMESPACE::putresults res2=cl->put(tran);

		if (res2->status == STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION("Transaction did not fail, as expected");

		std::cerr << "Expected error: " << x::tostring(res2->status)
			  << std::endl;
	}

}

static void test11(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	for (auto n:tnodes)
	{
		newtran tr=n->repo->newtransaction();

		(*tr->newobj(STASHER_NAMESPACE::client::base
			     ::maxobjects)->getostream())
			<< "10" << std::flush;

		(*tr->newobj(STASHER_NAMESPACE::client::base
			     ::maxobjectsize)->getostream())
			<< (1024 * 1024) << std::flush;

		n->repo->begin_commit(tr->finalize(),
				      x::eventfd::create())->commit();

	}

	t.startmastercontrolleron0_int(tnodes);


	{	
		STASHER_NAMESPACE::client cl=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

		cl->debugSetLimits(STASHER_NAMESPACE::userinit(10, 100,
							       1024 * 1024,
							       8192, 10));

		STASHER_NAMESPACE::client::base::transaction tran=STASHER_NAMESPACE::client::base::transaction::create();

		for (size_t i=0; i<20; i++)
		{
			std::ostringstream o;

			o << "obj" << i;

			tran->newobj(o.str(), "val");
		}

		STASHER_NAMESPACE::putresults res=cl->put(tran);

		if (res->status == STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION("Bad transaction did not fail (>10 obj put)");

		std::cerr << "Expected error: " << x::tostring(res->status) << std::endl;
	}

	{	
		STASHER_NAMESPACE::client cl=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

		cl->debugSetLimits(STASHER_NAMESPACE::userinit(10, 100,
							       1024 * 1024,
							       16384, 10));

		STASHER_NAMESPACE::client::base::transaction tran=STASHER_NAMESPACE::client::base::transaction::create();

		tran->newobj("obj", std::string(10000, 'x'));

		STASHER_NAMESPACE::putresults res=cl->put(tran);

		if (res->status == STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION("Bad transaction did not fail (chunk > 8192)");

		std::cerr << "Expected error: " << x::tostring(res->status) << std::endl;
	}

	{	
		STASHER_NAMESPACE::client cl=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

		cl->debugSetLimits(STASHER_NAMESPACE::userinit(10, 100,
							       1024 * 1024 * 10,
							       16384, 10));

		STASHER_NAMESPACE::client::base::transaction tran=STASHER_NAMESPACE::client::base::transaction::create();

		x::fd fd=x::fd::base::tmpfile();

		(*fd->getostream()) << std::string(1024 * 1023, 'x')
				    << std::flush;

		tran->newobj("obj1", fd);
		tran->newobj("obj2", fd);

		STASHER_NAMESPACE::putresults res=cl->put(tran);

		if (res->status == STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION("Bad transaction did not fail (3)");

		std::cerr << "Expected error: " << x::tostring(res->status) << std::endl;
	}

	{	
		STASHER_NAMESPACE::client cl=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

		STASHER_NAMESPACE::client::base::transaction tran=STASHER_NAMESPACE::client::base::transaction::create();

		tran->newobj(std::string(10, '/'), "obj");

		STASHER_NAMESPACE::putresults res=cl->put(tran);

		if (res->status == STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION("Bad transaction did not fail (4)");

		std::cerr << "Expected error: " << x::tostring(res->status) << std::endl;
	}
}

static void test12(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	t.startmastercontrolleron0_int(tnodes);

	STASHER_NAMESPACE::client cl=
		STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	{
		STASHER_NAMESPACE::client::base::transaction tran=
			STASHER_NAMESPACE::client::base::transaction::create();

		tran->newobj("etc/foo", "Not allowed\n"); // [WRITEPRIVILEGES]
		STASHER_NAMESPACE::putresults res=cl->put(tran);

		if (res->status != STASHER_NAMESPACE::req_eperm_stat)
			throw EXCEPTION("[WRITEPRIVILEGES] failed (1)");
	}

	cl=STASHER_NAMESPACE::client::base::admin(tstnodes::getnodedir(0));

	{
		STASHER_NAMESPACE::client::base::transaction tran=
			STASHER_NAMESPACE::client::base::transaction::create();

		tran->newobj("etc/foo", "Not allowed\n");
		STASHER_NAMESPACE::putresults res=cl->put(tran);

		if (res->status != STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION("[WRITEPRIVILEGES] failed (2)");
	}
}

void test13(const char *argv0)
{
	x::dir::base::rmrf("conftestdir.tst");
	mkdir("conftestdir.tst", 0777);
	{
		std::ofstream o("conftestdir.tst/test.tst");

		o << "PATH \"" << argv0 << "\"" << std::endl
		  << "ROOT tree" << std::endl
		  << "RW rw rwpath" << std::endl
		  << "RO ro ropath" << std::endl;
		o.close();

		o.open("conftestdir.tst/#ignore");
		o.close();
		o.open("conftestdir.tst/~ignore");
		o.close();
	}

	nsmap::local_map_t map;

	nsmap::get_local_map("conftestdir.tst", map);
	auto p=map.begin();

	if (p == map.end() || p->second.first[""] != "tree"
	    || p->second.first["rw"] != "rwpath"
	    || p->second.second["ro"] != "ropath")
		throw EXCEPTION("get_local_map test failed");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		x::dir::base::rmrf("conftestdir.tst");
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
		test8(0);
		test8(-1);
		std::cout << "test9" << std::endl;
		test9();
		std::cout << "test9b" << std::endl;
		test9b();
		x::dir::base::rmrf("conftestdir.tst");

		tstnodes nodes(2);

		std::cout << "test10" << std::endl;
		test10(nodes);
		std::cout << "test11" << std::endl;
		test11(nodes);
		std::cout << "test12" << std::endl;
		test12(nodes);
		test13(argv[0]);
		x::dir::base::rmrf("conftestdir.tst");
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

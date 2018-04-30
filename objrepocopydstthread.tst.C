/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo.H"
#include "tobjrepo.H"
#include "tran.H"
#include "newtran.H"
#include "trancommit.H"
#include "objrepocopy.H"
#include "objrepocopysrcinterface.H"
#include "objrepocopydstinterface.H"
#include <x/threads/run.H>
#include <x/destroy_callback.H>
#include <x/options.H>

static void (*wait_lock_hook)()=0;
static void (*after_lock_hook)()=0;

#define DST_DEBUG_WAIT_LOCK()						\
	do { if (wait_lock_hook) (*wait_lock_hook)(); } while (0)

#define DST_DEBUG_AFTER_LOCK()						\
	do { if (after_lock_hook) (*after_lock_hook)(); } while (0)

#include "objrepocopydstthread.C"
#include "objrepocopydst.C"

#include <sstream>
#include <iostream>

static x::uuid mkobj(const tobjrepo &repo,
		     const std::string &objname)
{
	newtran tran(repo->newtransaction());

	tran->newobj(objname);

	x::uuid uuid(tran->finalize());

	repo->begin_commit(uuid, x::eventfd::create())->commit();

	return uuid;
}

class srcemu : public objrepocopysrcinterfaceObj {

public:

	srcemu() : catchmsg(false), caughtmsg(false) {}
	~srcemu() {}

	std::mutex mutex;
	std::condition_variable cond;

	objuuidlist slavelist;
	bool slavelistready;
	bool slavelistdone;

	std::mutex catchmutex;
	std::condition_variable catchcond;

	bool catchmsg;
	bool caughtmsg;

	void clear()
	{
		std::lock_guard<std::mutex> lock(mutex);

		slavelist=objuuidlist();
		slavelistready=false;
		slavelistdone=false;
	}

	void event(const objrepocopy::batonresponse &msg) override
	{
	}

	void event(const objrepocopy::slavelist &msg)
		override
	{
		std::unique_lock<std::mutex> lock(mutex);

		slavelist=msg.uuids;
		cond.notify_all();
	}

	void event(const objrepocopy::slavelistready &msg)
		override
	{
		{
			std::unique_lock<std::mutex> lock(catchmutex);

			if (catchmsg)
			{
				caughtmsg=true;

				catchcond.notify_all();

				while (catchmsg)
					catchcond.wait(lock);
			}
		}

		std::lock_guard<std::mutex> lock(mutex);

		slavelistready=true;
		cond.notify_all();
	}

	void event(const objrepocopy::slavelistdone &msg)
		override
	{
		std::lock_guard<std::mutex> lock(mutex);

		slavelistdone=true;
		cond.notify_all();
	}
};

static void test1()
{
	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	std::map<std::string, x::uuid> uuidmap, uuidmap2;

	for (size_t i=objuuidlistObj::default_chunksize.get()*2; i; --i)
	{
		std::stringstream ss;

		ss << "obj" << i;

		std::string name(ss.str());

		uuidmap[name]=mkobj(repo, name);
	}

	x::ptr<srcemu> src(x::ptr<srcemu>::create());

	objrepocopydstthreadptr
		test_thread(objrepocopydstthreadptr::create("srccopy"));

	boolref flag(boolref::create());

	x::destroy_callback monitor(x::destroy_callback::create());

	auto runthread=({
			auto mcguffin=x::ref<x::obj>::create();

			mcguffin->ondestroy([monitor]{monitor->destroyed();});

			x::start_threadmsgdispatcher(test_thread,
					start_threadmsgdispatcher_sync::create(),
					repo,
					x::weakptr<objrepocopysrcinterfaceptr>
					(src), flag, batonptr(),
					mcguffin);
		});

	try {

		src->clear();

		{
			objrepocopy::masterlist msg;

			msg.uuids=objuuidlist::create();

			msg.uuids->objuuids.insert(std::make_pair("obj1",
								  uuidmap
								  ["obj1"]));
			// [SAMEUUID]

			msg.uuids->objuuids.insert(std::make_pair("obj2",
								  x::uuid()));
			// [UUIDMISMATCH]

			msg.uuids->objuuids.insert(std::make_pair("objx",
								  x::uuid()));
			// [NOOBJECT]

			test_thread->event(msg);
		}

		{
			std::unique_lock<std::mutex> lock(src->mutex);

			while (src->slavelist.null())
				src->cond.wait(lock);
		}

		if (src->slavelist->objnouuids.size() != 1 ||
		    src->slavelist->objuuids.size() != 1 ||
		    src->slavelist->objnouuids.find("objx") ==
		    src->slavelist->objnouuids.end() ||
		    src->slavelist->objuuids.find("obj2") ==
		    src->slavelist->objuuids.end() ||
		    src->slavelist->objuuids.find("obj2")->second !=
		    uuidmap["obj2"])
			throw EXCEPTION("[SAMEUUID] [UUIDMISMATCH] [NOOBJECT] failed");

		src->clear();

		{
			objrepocopy::masterlistdone msg;

			test_thread->event(msg);
		}

		{
			std::unique_lock<std::mutex> lock(src->mutex);

			while (!src->slavelistready) // [RECVMASTERLISTDONE]
				src->cond.wait(lock);
		}

		src->clear();

		{
			objrepocopy::slaveliststart msg;

			test_thread->event(msg); // [RECVSLAVELISTSTART]
		}

		while (1)
		{
			{
				std::unique_lock<std::mutex>
					lock(src->mutex);

			again:
				if (src->slavelistdone)
					break;

				if (src->slavelist.null())
				{
					src->cond.wait(lock);
					goto again;
				}
			}

			uuidmap2.insert(src->slavelist->objuuids.begin(),
					src->slavelist->objuuids.end());

			src->clear();
			{
				objrepocopy::masterack msg;

				test_thread->event(msg); // [RECVMASTERACK]
			}
		}

		if (uuidmap != uuidmap2)
			throw EXCEPTION("uuidmap comparison failed");

		objrepocopy::copycomplete msg;

		test_thread->event(msg); // [RECVCOPYDONE]

		monitor->wait(); // [MCGUFFIN]

		if (!flag->flag)
			throw EXCEPTION("[MCGUFFIN] failed");

		runthread->wait();

	} catch (...)
	{
		test_thread->stop();
		runthread->wait();
		throw;
	}
}

static void test2()
{
	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	x::ptr<srcemu> src(x::ptr<srcemu>::create());

	objrepocopydstthreadptr
		test_thread(objrepocopydstthreadptr::create("dstcopy"));

	auto runthread=x::start_threadmsgdispatcher(test_thread,
				       start_threadmsgdispatcher_sync::create(),
				       repo,
				       x::weakptr<objrepocopysrcinterfaceptr>(src),
				       boolref::create(),
				       batonptr(), x::ptr<x::obj>());

	try {

		src=x::ptr<srcemu>(); // [SRCGONE]

		objrepocopy::masterlist msg;

		msg.uuids=objuuidlist::create();

		msg.uuids->objuuids.insert(std::make_pair("obj1", x::uuid()));
		test_thread->event(msg);
		runthread->wait();

	} catch (...)
	{
		test_thread->stop();
		runthread->wait();
		throw;
	}
}

static void test3()
{
	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	x::ptr<srcemu> src(x::ptr<srcemu>::create());

	src->catchmsg=true;

	objrepocopydstthreadptr
		test_thread(objrepocopydstthreadptr::create("objrepocopydst"));


	boolref flag(boolref::create());

	x::destroy_callback monitor(x::destroy_callback::create());

	auto runthread=({
			auto mcguffin=x::ref<x::obj>::create();

			mcguffin->ondestroy([monitor]{monitor->destroyed();});

			x::start_threadmsgdispatcher(test_thread,
					start_threadmsgdispatcher_sync::create(),
					repo,
					x::weakptr<objrepocopysrcinterfaceptr>(src),
					flag, batonptr(),
					mcguffin);
		});

	try {

		objrepocopy::masterlistdone msg;

		test_thread->event(msg);

		{
			std::unique_lock<std::mutex> lock(src->catchmutex);

			while (!src->caughtmsg)
				src->catchcond.wait(lock);

			test_thread->stop(); // [RECVSTOPREQUEST]
			src->catchmsg=false;
			src->catchcond.notify_all();
		}

		monitor->wait();

		if (flag->flag)
			throw EXCEPTION("[RECVSTOPREQUEST] failed");

		runthread->wait();

	} catch (...)
	{
		test_thread->stop();
		runthread->wait();
		throw;
	}
}

class test45_sync {

public:

	std::mutex mutex;
	std::condition_variable cond;

	bool semaphore;

};

static test45_sync *test45_sync_ptr;

static void test45_sync_func()
{
	std::unique_lock<std::mutex> lock(test45_sync_ptr->mutex);

	test45_sync_ptr->semaphore=true;

	test45_sync_ptr->cond.notify_all();

	while (test45_sync_ptr->semaphore)
		test45_sync_ptr->cond.wait(lock);
}

static void test4()
{
	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	x::ptr<srcemu> src(x::ptr<srcemu>::create());

	objrepocopydstthreadptr
		test_thread(objrepocopydstthreadptr::create("objrepocopydstthread"));

	auto runthread=x::start_threadmsgdispatcher(test_thread,
				       start_threadmsgdispatcher_sync::create(),
				       repo,
				       x::weakptr<objrepocopysrcinterfaceptr>(src),
				       boolref::create(),
				       batonptr(),
				       x::ptr<x::obj>());

	test45_sync syncinfo;

	syncinfo.semaphore=false;
	test45_sync_ptr=&syncinfo;

	try {

		src->clear();

		tobjrepoObj::lockentryptr_t l;

		{
			std::set<std::string> lock;

			lock.insert("obj1");

			l=repo->lock(lock, x::eventfd::create());
		}

		{
			objrepocopy::masterlist msg;

			msg.uuids=objuuidlist::create();

			msg.uuids->objuuids.insert(std::make_pair("obj1",
								  x::uuid()));
			test_thread->event(msg);
		}

		{
			std::unique_lock<std::mutex>
				lock(syncinfo.mutex);

			while (!syncinfo.semaphore)
				syncinfo.cond.wait(lock);

			test_thread->stop();

			syncinfo.semaphore=false;
			syncinfo.cond.notify_all();
		}

		runthread->wait();

	} catch (...)
	{
		test_thread->stop();
		runthread->wait();
		throw;
	}
}

static void test5()
{
	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	x::ptr<srcemu> src(x::ptr<srcemu>::create());

	objrepocopydstthreadptr
		test_thread(objrepocopydstthreadptr::create("dst"));

	auto runthread=x::start_threadmsgdispatcher(test_thread,
				       start_threadmsgdispatcher_sync::create(),
				       repo,
				       x::weakptr<objrepocopysrcinterfaceptr>(src),
				       boolref::create(),
				       batonptr(),
				       x::ptr<x::obj>());

	test45_sync syncinfo;

	syncinfo.semaphore=false;
	test45_sync_ptr=&syncinfo;

	try {

		src->clear();

		{
			objrepocopy::masterlist msg;

			msg.uuids=objuuidlist::create();

			msg.uuids->objuuids.insert(std::make_pair("obj1",
								  x::uuid()));
			test_thread->event(msg);
		}

		{
			std::unique_lock<std::mutex>
				lock(syncinfo.mutex);

			while (!syncinfo.semaphore)
				syncinfo.cond.wait(lock);

			test_thread->stop();

			syncinfo.semaphore=false;
			syncinfo.cond.notify_all();
		}

		runthread->wait();

	} catch (...)
	{
		test_thread->stop();
		runthread->wait();
		throw;
	}
}

static void test6()
{
	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	x::ptr<srcemu> src(x::ptr<srcemu>::create());

	objrepocopydst test_thread(objrepocopydst::create());

	test_thread->start(repo, src,
			   boolref::create(),
			   batonptr(),
			   x::ptr<x::obj>());
}

class test7_fakesrc : public objrepocopysrcinterfaceObj {

public:

	test7_fakesrc() {}

	~test7_fakesrc(){}

	std::string batonp;
	std::mutex mutex;
	std::condition_variable cond;

	void event(const objrepocopy::batonresponse &msg)
		override
	{
		std::lock_guard<std::mutex> lock(mutex);
		batonp=msg.uuid;
		cond.notify_all();
	}

	void event(const objrepocopy::slavelist &msg)
		override
	{
	}

	void event(const objrepocopy::slavelistready &msg)
		override
	{
	}

	void event(const objrepocopy::slavelistdone &msg)
		override
	{
	}
};

void test7()
{
	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	x::ptr<test7_fakesrc> src;
	objrepocopydstptr test_thread;
	batonptr batonp;
	x::uuid batonuuid;


	src=x::ptr<test7_fakesrc>::create();
	test_thread=objrepocopydstptr::create();

	batonp=batonptr::create("a", x::uuid(), "b", batonuuid);

	test_thread->start(repo, src,
			   boolref::create(),
			   batonp,
			   x::ptr<x::obj>());

	try {
		test_thread->event(objrepocopy::batonrequest());

		{
			std::unique_lock<std::mutex> lock(src->mutex);

			while (src->batonp.size() == 0)
				src->cond.wait(lock);

			if (src->batonp != x::tostring(batonuuid))
				throw EXCEPTION("test7 failed");

			// [BATONDSTCOPYRELEASECOMPLETE]
			test_thread->event(objrepocopy::copycomplete());

			x::destroy_callback
				monitor(x::destroy_callback::create());

			batonp->ondestroy([monitor]{monitor->destroyed();});

			x::weakptr<batonptr > wbaton=batonp;

			batonp=batonptr();

			monitor->wait();
		}

		test_thread->stop();
		test_thread->wait();

	} catch (...)
	{
		test_thread->stop();
		test_thread->wait();
		throw;
	}


	src=x::ptr<test7_fakesrc>::create();
	test_thread=objrepocopydstptr::create();

	batonp=batonptr::create("a", x::uuid(), "b", batonuuid);

	test_thread->start(repo, src,
			   boolref::create(),
			   batonp,
			   x::ptr<x::obj>());

	try {
		test_thread->event(objrepocopy::batonrequest());

		{
			std::unique_lock<std::mutex> lock(src->mutex);

			while (src->batonp.size() == 0)
				src->cond.wait(lock);

			if (src->batonp != x::tostring(batonuuid))
				throw EXCEPTION("test7 failed");

			objrepocopy::masterlist msg;

			msg.uuids=objuuidlist::create();

			// [BATONDSTCOPYRELEASESLAVELIST]
			test_thread->event(msg);

			x::destroy_callback
				monitor(x::destroy_callback::create());

			batonp->ondestroy([monitor]{monitor->destroyed();});

			x::weakptr<batonptr > wbaton=batonp;

			batonp=batonptr();

			monitor->wait();
		}

		test_thread->stop();
		test_thread->wait();

	} catch (...)
	{
		test_thread->stop();
		test_thread->wait();
		throw;
	}
}


int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	ALARM(30);

	try {
		x::dir::base::rmrf("conftestdir.tst");

		std::cerr << "test1" << std::endl;
		test1();
		std::cerr << "test2: expected errors to follow:" << std::endl;
		test2();
		std::cerr << "test3" << std::endl;
		test3();

		std::cerr << "test4" << std::endl;
		wait_lock_hook=test45_sync_func;
		test4();
		wait_lock_hook=NULL;

		std::cerr << "test5" << std::endl;
		after_lock_hook=test45_sync_func;
		test5();
		std::cerr << "test6" << std::endl;
		after_lock_hook=NULL;
		test6();

		std::cerr << "test7" << std::endl;
		test7();
		x::dir::base::rmrf("conftestdir.tst");
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

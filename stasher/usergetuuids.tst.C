/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "stasher/client.H"
#include "stasher/userinit.H"
#include <x/options.H>
#include <x/fditer.H>
#include <x/threads/run.H>
#include <x/deserialize.H>

static std::mutex test34_mutex;
static std::condition_variable test34_cond;
static bool test34_enabled=false;
static bool test34_notinquorum;

#define DEBUG_LOCALCONNECTION_NOTQUORUM() do {			\
		std::unique_lock<std::mutex> lock(test34_mutex);		\
		if (test34_enabled)				\
		{						\
			test34_notinquorum=true;			\
			std::cerr << "waiting" << std::endl;	\
			test34_cond.notify_all();			\
		}						\
	} while(0)

#include "localconnection.C"

static void test1(tstnodes &t)
{
	std::cerr << "test1" << std::endl;

	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	t.startmastercontrolleron0_int(tnodes);

	STASHER_NAMESPACE::client
		cl0=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));
	STASHER_NAMESPACE::client
		cl1=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(1));

	x::uuid obj1uuid=({
			STASHER_NAMESPACE::client::base::transaction tran=
				STASHER_NAMESPACE::client::base::transaction::create();

			tran->newobj("obj1", "obj1_value\n");
			STASHER_NAMESPACE::putresults res=cl0->put(tran);

			if (res->status != STASHER_NAMESPACE::req_processed_stat)
				throw EXCEPTION(x::tostring(res->status));

			res->newuuid;
		});

	std::cerr << "New transaction: " << x::tostring(obj1uuid)
		  << std::endl;

	{
		std::set<std::string> set;

		set.insert("obj1");
		set.insert("obj2");

		STASHER_NAMESPACE::contents
			res=cl1->get(set, true)->objects;

		if (!res->succeeded)
			throw EXCEPTION(res->errmsg);

		std::string line;

		if (res->size() != 1 ||
		    (*res)["obj1"].uuid != obj1uuid ||
		    (std::getline(*(*res)["obj1"].fd->getistream(), line),
		     line) != "obj1_value")
		{
			throw EXCEPTION("test1 failed");
		}
	}

	{
		std::set<std::string> set;

		for (size_t i=0; i<100; ++i)
		{
			std::ostringstream o;

			o << "obj" << i;

			set.insert(o.str());
		}

		STASHER_NAMESPACE::contents res=cl1->get(set)->objects;

		if (res->succeeded)
			throw EXCEPTION("Bad transaction did not fail, for some reason");
		std::cerr << "Expected error: " << res->errmsg << std::endl;
	}

	{
		std::set<std::string> set;

		set.insert(std::string(1000, 'x'));

		STASHER_NAMESPACE::contents res=cl1->get(set)->objects;

		if (res->succeeded)
			throw EXCEPTION("Bad transaction did not fail, for some reason");
		std::cerr << "Expected error: " << res->errmsg << std::endl;
	}

	{
		std::set<std::string> set;

		set.insert(std::string(10, '/'));

		STASHER_NAMESPACE::contents res=cl1->get(set)->objects;

		if (res->succeeded)
			throw EXCEPTION("Bad transaction did not fail, for some reason");
		std::cerr << "Expected error: " << res->errmsg << std::endl;
	}

	{
		STASHER_NAMESPACE::client cl2=
			STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

		cl2->debugSetLimits(STASHER_NAMESPACE::userinit(10, 100,
								1024 * 1024 * 10,
								16384, 10));

		std::set<std::string> set;

		for (size_t i=0; i<50; ++i)
		{
			std::ostringstream o;

			o << "obj" << i;

			set.insert(o.str());
		}

		STASHER_NAMESPACE::contents res=cl2->get(set)->objects;

		if (res->succeeded)
			throw EXCEPTION("Bad transaction did not fail, for some reason");
		std::cerr << "Expected error: " << res->errmsg << std::endl;
	}
}

static void test2(tstnodes &t)
{
	std::cerr << "test2" << std::endl;

	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	t.startmastercontrolleron0_int(tnodes);

	STASHER_NAMESPACE::client cl=
		STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	x::uuid obj1uuid=({
			std::set<std::string> set;

			set.insert("obj1");

			STASHER_NAMESPACE::contents
				res=cl->get(set, false)->objects;

			if (!res->succeeded)
				throw EXCEPTION(res->errmsg);

			(*res)["obj1"].uuid;
		});

	x::uuid obj2uuid=({
			STASHER_NAMESPACE::client::base::transaction tran=
				STASHER_NAMESPACE::client::base::transaction::create();

			tran->newobj("obj2", "obj2_value\n");
			tran->newobj("obj3", "obj3_value\n");
			STASHER_NAMESPACE::putresults res=cl->put(tran);

			if (res->status != STASHER_NAMESPACE::req_processed_stat)
				throw EXCEPTION(x::tostring(res->status));

			res->newuuid;
		});

	{
		std::set<std::string> set;

		set.insert("obj3");
		set.insert("obj1");
		set.insert("obj4");

		STASHER_NAMESPACE::contents
			res=cl->get(set, false)->objects;

		if (!res->succeeded)
			throw EXCEPTION(res->errmsg);

		if (res->size() != 2 ||
		    (*res)["obj1"].uuid != obj1uuid ||
		    (*res)["obj3"].uuid != obj2uuid)
			throw EXCEPTION("test2 failed (1)");
	}


	{
		std::set<std::string> set;

		set.insert("obj2");
		set.insert("obj1");

		STASHER_NAMESPACE::contents
			res=cl->get(set, true)->objects;

		if (!res->succeeded)
			throw EXCEPTION(res->errmsg);

		std::string line;

		if (res->size() != 2 ||
		    (std::getline(*(*res)["obj1"].fd->getistream(), line),
		     line) != "obj1_value" ||
		    (std::getline(*(*res)["obj2"].fd->getistream(), line),
		     line) != "obj2_value")
			throw EXCEPTION("test2 failed (2)");
	}

	x::uuid obj3uuid=({
			STASHER_NAMESPACE::client::base::transaction tran=
				STASHER_NAMESPACE::client::base::transaction::create();

			tran->delobj("obj2", obj2uuid);
			tran->updobj("obj3", obj2uuid, "obj3_newvalue\n");
			STASHER_NAMESPACE::putresults res=cl->put(tran);

			if (res->status != STASHER_NAMESPACE::req_processed_stat)
				throw EXCEPTION(x::tostring(res->status));

			res->newuuid;
		});

	{
		std::set<std::string> set;

		set.insert("obj1");
		set.insert("obj2");
		set.insert("obj3");

		STASHER_NAMESPACE::contents
			res=cl->get(set, true)->objects;

		if (!res->succeeded)
			throw EXCEPTION(res->errmsg);

		std::string line;

		STASHER_NAMESPACE::contents::base::map_t &objs=*res;

		if (objs.size() != 2 ||
		    objs["obj1"].uuid != obj1uuid ||
		    objs["obj3"].uuid != obj3uuid ||
		    (std::getline(*objs["obj1"].fd->getistream(), line),
		     line) != "obj1_value" ||
		    (std::getline(*objs["obj3"].fd->getistream(), line),
		     line) != "obj3_newvalue")
			throw EXCEPTION("test2 failed (3)");
	}
	cl->disconnect(); // [CLIENTDISCONNECT]
}

class test3_thr : public x::stoppableObj {

public:
	tstnodes::noderef node0;
	STASHER_NAMESPACE::client cl;

	test3_thr(tstnodes::noderef node0Arg,
		  const STASHER_NAMESPACE::client &clArg)
		: node0(node0Arg), cl(clArg) {}

	~test3_thr() noexcept {}

	void run()
	{
		{
			std::unique_lock<std::mutex> lock(test34_mutex);

			while (!test34_notinquorum)
				test34_cond.wait(lock);
		}

		std::cerr << cl->getserverstatus()->report << std::endl;

		node0->listener->connectpeers();
	}

	void stop()
	{
		std::unique_lock<std::mutex> lock(test34_mutex);

		test34_notinquorum=true;
		test34_cond.notify_all();
	}
};

// Testing the GETs are processed only when in quorum

static void test3(tstnodes &t, size_t which)
{
	std::cerr << "test 3, part " << which+1 << std::endl;

	{
		std::vector<tstnodes::noderef> tnodes;

		t.init(tnodes);
		t.startmastercontrolleron0_int(tnodes);
	}

	{
		std::unique_lock<std::mutex> lock(test34_mutex);
		test34_enabled=true;
		test34_notinquorum=false;
	}

	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	tnodes[0]->start(true);
	tnodes[1]->start(true);
	tnodes[0]->debugWaitFullQuorumStatus(false);
	tnodes[1]->debugWaitFullQuorumStatus(false);

	std::cerr << "started" << std::endl;

	STASHER_NAMESPACE::client cl=
		STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(which));

	std::cerr << "connected" << std::endl;

	x::ref<test3_thr> t3(x::ref<test3_thr>::create(tnodes[1], cl));

	tnodes[0]->tracker->start(t3);

	{
		std::set<std::string> set;

		set.insert("foobar");

		cl->get(set, which > 0, true); // [FORCEGETADMIN]

		// test3 waits for the hook, then calls connectpeers().
		// Connection is made, quorum gets established, get()
		// succeeds.
	}

	std::cerr << "retrieved" << std::endl;

	{
		std::unique_lock<std::mutex> lock(test34_mutex);
		test34_enabled=false;

		if (!test34_notinquorum)
			throw EXCEPTION("test3 failed");
	}
}

static void test4(tstnodes &t)
{
	std::cerr << "test 4" << std::endl;

	{
		std::vector<tstnodes::noderef> tnodes;

		t.init(tnodes);
		t.startmastercontrolleron0_int(tnodes);
	}

	{
		std::unique_lock<std::mutex> lock(test34_mutex);
		test34_enabled=true;
		test34_notinquorum=false;
	}

	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	tnodes[0]->start(true);
	tnodes[1]->start(true);
	tnodes[0]->debugWaitFullQuorumStatus(false);
	tnodes[1]->debugWaitFullQuorumStatus(false);

	std::cerr << "started" << std::endl;

	STASHER_NAMESPACE::client cl=
		STASHER_NAMESPACE::client::base::admin(tstnodes::getnodedir(0));

	std::cerr << "connected" << std::endl;

	{
		std::set<std::string> set;

		set.insert("foobar");

		cl->get(set, false, true);
	}

	std::cerr << "retrieved" << std::endl;

	{
		std::unique_lock<std::mutex> lock(test34_mutex);
		test34_enabled=false;

		if (test34_notinquorum)
			throw EXCEPTION("test3 failed");
	}
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		tstnodes nodes(2);

		test1(nodes);
		test2(nodes);
		test3(nodes, 0);
		test3(nodes, 1);
		test4(nodes);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

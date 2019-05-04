/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "stasher/client.H"
#include <x/options.H>
#include <x/destroy_callback.H>
#include <map>
#include <set>
#include <chrono>

struct created_destroyed_count_info {

	size_t hier_created_cnt;
	size_t hier_destroyed_cnt;

	created_destroyed_count_info()
		: hier_created_cnt(0), hier_destroyed_cnt(0)
	{
	}
};

typedef x::mpcobj<created_destroyed_count_info> created_destroyed_count_mpc_t;

struct created_destroyed_count_t {

public:
	created_destroyed_count_mpc_t c;

	void created()
	{
		created_destroyed_count_mpc_t::lock lock(c);
		++lock->hier_created_cnt;
		lock.notify_all();
	}

	void destroyed()
	{
		created_destroyed_count_mpc_t::lock lock(c);
		++lock->hier_destroyed_cnt;
		lock.notify_all();
	}

	void wait_all()
	{
		std::cout << "End of test, waiting for everything to be unsubscribed" << std::endl;

		created_destroyed_count_mpc_t::lock lock(c);

		if (!lock.wait_for(std::chrono::seconds(10), [&lock]
				   {
					   std::cout << "Subscribed "
						     << lock->hier_created_cnt
						     << " times, unsubscribed "
						     << lock->hier_destroyed_cnt
						     << " times" << std::endl;
					   return lock->hier_created_cnt ==
						   lock->hier_destroyed_cnt;
				   }))
		{
			throw EXCEPTION("Created/destroyed hierarchy node mismatch");
		}
	}
};

created_destroyed_count_t cd;
static bool hier_exception_flag=false;

#define TEST_SUBSCRIBED_HIER_CREATED() (cd.created())
#define TEST_SUBSCRIBED_HIER_DESTROYED() (cd.destroyed())
#define TEST_SUBSCRIBED_HIER_EXCEPTION() (hier_exception_flag=true)

#include "localconnection.C"

class test1subscriber
	: public STASHER_NAMESPACE::client::base::subscriberObj {

	std::map<std::string, std::set<std::string> > updateset;
	std::mutex mutex;
	std::condition_variable cond;

	std::string name;

public:
	test1subscriber(const std::string &nameArg) : name(nameArg) {}
	~test1subscriber() {}

	void updated(const std::string &objname,
		     const std::string &suffix) override
	{
		std::unique_lock<std::mutex> lock(mutex);

		std::cout << name << ": received update: " << objname
			  << ", suffix " << suffix << std::endl;

		updateset[objname].insert(suffix);
		cond.notify_all();
	}

	void wait(const std::string &objname)
	{
		std::unique_lock<std::mutex> lock(mutex);

		cond.wait(lock,
			  [this, &objname]
			  {
				  return updateset.find(objname)
					  != updateset.end();
			  });
	}

	bool received(const std::string &objname)
	{
		std::unique_lock<std::mutex> lock(mutex);

		return updateset.find(objname) != updateset.end();
	}

	std::string received()
	{
		std::unique_lock<std::mutex> lock(mutex);

		std::ostringstream o;

		for (auto received: updateset)
		{
			for (auto suffix: received.second)
			{
				o << received.first << suffix
				  << "\n";
			}
		}

		return o.str();
	}

	void clear()
	{
		std::unique_lock<std::mutex> lock(mutex);

		updateset.clear();
	}
};

static void do_test1(const STASHER_NAMESPACE::client &cl0,
		     const STASHER_NAMESPACE::client &cl1);

static void test1(tstnodes &t, const char *root_ns)
{
	std::cerr << "test1 (root namespace=\""
		  << root_ns << "\")" << std::endl;

	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes, root_ns);
	t.startmastercontrolleron0_int(tnodes);

	STASHER_NAMESPACE::client cl0=
		STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	STASHER_NAMESPACE::client cl1=
		STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(1));

	do_test1(cl0, cl1);
	cd.wait_all();
}

static void do_test1(const STASHER_NAMESPACE::client &cl0,
		     const STASHER_NAMESPACE::client &cl1)
{
	std::cerr << "Subscribing" << std::endl;

	x::ptr<test1subscriber> rootsub=x::ptr<test1subscriber>::create("rootsub");

	// [SUBSCRIBE]
	STASHER_NAMESPACE::subscriberesultsptr
		rootsubres=cl0->subscribe("", rootsub);

	if (rootsubres->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION("subscribe(\"\") failed");

	x::ptr<test1subscriber> obj1sub=x::ptr<test1subscriber>::create("obj1sub");

	STASHER_NAMESPACE::subscriberesultsptr
		obj1subres=cl0->subscribe("obj1", obj1sub);

	if (obj1subres->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION("subscribe(\"obj1\") failed");

	x::ptr<test1subscriber> obj1hiersub=x::ptr<test1subscriber>::create("obj1hiersub");

	STASHER_NAMESPACE::subscriberesultsptr
		obj1hiersubres=cl0->subscribe("obj1/", obj1hiersub);

	if (obj1hiersubres->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION("subscribe(\"obj1/\") failed");


	{
		STASHER_NAMESPACE::subscriberesults
			errsubres=cl0->subscribe("err//or",
						x::ptr<test1subscriber>
						::create("error"));

		if (errsubres->status != STASHER_NAMESPACE::req_badname_stat)
			throw EXCEPTION("Managed to subscribed to a bad name");
	}

	{
		STASHER_NAMESPACE::client::base::transaction tran=
			STASHER_NAMESPACE::client::base::transaction::create();

		tran->newobj("obj1", "obj1_value");

		STASHER_NAMESPACE::putresults res=cl1->put(tran);

		if (res->status != STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION(x::to_string(res->status));
	}

	std::cerr << "Checking callback for obj1: root" << std::endl;
	rootsub->wait("");
	std::cerr << "Checking callback for obj1: obj1" << std::endl;
	obj1sub->wait("obj1");
	if (obj1hiersub->received() != "")
		throw EXCEPTION("Unexpected update for obj1/");

	rootsub->clear();
	obj1sub->clear();

	{
		STASHER_NAMESPACE::client::base::transaction tran=
			STASHER_NAMESPACE::client::base::transaction::create();

		tran->newobj("obj12", "obj12_value");

		STASHER_NAMESPACE::putresults res=cl1->put(tran);

		if (res->status != STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION(x::to_string(res->status));
	}

	// [USERSUBSCRIBEHIER]
	std::cerr << "Checking callback for obj1: root" << std::endl;
	rootsub->wait("");

	if (obj1sub->received() != "" ||
	    obj1hiersub->received() != "")
		throw EXCEPTION("Unexpected update for obj1/");

	rootsub->clear();

	std::cerr << "Updating obj1/2" << std::endl;

	{
		STASHER_NAMESPACE::client::base::transaction tran=
			STASHER_NAMESPACE::client::base::transaction::create();

		tran->newobj("obj1/2", "obj12_value");

		STASHER_NAMESPACE::putresults res=cl1->put(tran);

		if (res->status != STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION(x::to_string(res->status));
	}

	obj1hiersub->wait("obj1/");
	rootsub->wait("");

	if (obj1sub->received() != "")
		throw EXCEPTION("Unexpected update for obj1/2 for root node");

	if (rootsub->received() != "obj1/2\n")
		throw EXCEPTION("[USERSUBSCRIBEHIER] recursive failed: "
				+ rootsub->received());

	if (obj1hiersub->received() != "obj1/2\n") // [USERSUBSCRIBESUFFIX]
		throw EXCEPTION("Did not received expected update for obj1/2");

	x::destroy_callback cbroot=x::destroy_callback::create();
	rootsub->ondestroy([cbroot]{cbroot->destroyed();});
	rootsub=x::ptr<test1subscriber>();

	x::destroy_callback cbobj1=x::destroy_callback::create();
	obj1sub->ondestroy([cbobj1]{cbobj1->destroyed();});
	obj1sub=x::ptr<test1subscriber>();

	x::destroy_callback cbobj1hier=x::destroy_callback::create();
	obj1hiersub->ondestroy([cbobj1hier]{cbobj1hier->destroyed();});
	obj1hiersub=x::ptr<test1subscriber>();

	std::cerr << "Unsubscribing" << std::endl;
	rootsubres=STASHER_NAMESPACE::subscriberesultsptr();
	obj1subres=STASHER_NAMESPACE::subscriberesultsptr();
	obj1hiersubres=STASHER_NAMESPACE::subscriberesultsptr();
	cbroot->wait();
	cbobj1->wait();
	cbobj1hier->wait(); // [UNSUBSCRIBE]
}

static void do_test2(const STASHER_NAMESPACE::client &cl0);

static void test2(tstnodes &t)
{
	std::cerr << "test2" << std::endl;
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	STASHER_NAMESPACE::client cl0=
		STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	do_test2(cl0);
	cd.wait_all();
}

static void do_test2(const STASHER_NAMESPACE::client &cl0)
{
	std::list<STASHER_NAMESPACE::subscriberesults> resList;
	size_t n=0;

	x::ptr<test1subscriber> dummy=x::ptr<test1subscriber>::create("dummy");

	STASHER_NAMESPACE::subscriberesultsptr res;

	do
	{
		std::ostringstream o;

		o << "obj" << ++n;


		res=cl0->subscribe(o.str(), dummy);
		resList.push_back(res);

		if (n > 10000)
			throw EXCEPTION("Can't find a subscription limit?");

	} while (res->status == STASHER_NAMESPACE::req_processed_stat);

	// [SUBSCRIBE2MANY]
	if (res->status != STASHER_NAMESPACE::req_toomany_stat)
		throw EXCEPTION(x::to_string(res->status));
}

static void do_test3(const STASHER_NAMESPACE::client &cl0);

static void test3(tstnodes &t)
{
	std::cerr << "test3" << std::endl;
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes, { {"", ""} },
	       {
		       {"view1", "common1"},
		       {"view2", "common1"},
		       {"view3", "common1"},
			       });

	t.startmastercontrolleron0_int(tnodes);

	std::cout << "Ready" << std::endl;

	STASHER_NAMESPACE::client cl0=
		STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));
	do_test3(cl0);
	cd.wait_all();
}

static void do_test3(const STASHER_NAMESPACE::client &cl0)
{
	x::ref<test1subscriber> view1sub=x::ptr<test1subscriber>::create("view1sub");
	x::ref<test1subscriber> view2sub=x::ptr<test1subscriber>::create("view2sub");
	x::ref<test1subscriber> view3sub=x::ptr<test1subscriber>::create("view3sub");

	auto view1res=cl0->subscribe("view1/", view1sub);
	auto view2res=cl0->subscribe("view2/", view2sub);
	auto view3res=cl0->subscribe("view3/", view3sub);

	if (view1res->status != STASHER_NAMESPACE::req_processed_stat ||
	    view2res->status != STASHER_NAMESPACE::req_processed_stat ||
	    view3res->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION("subscribe(\"\") failed");

	{
		STASHER_NAMESPACE::client::base::transaction tran=
			STASHER_NAMESPACE::client::base::transaction::create();

		tran->newobj("common1/sub/obj", "obj_value");

		STASHER_NAMESPACE::putresults res=cl0->put(tran);

		if (res->status != STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION(x::to_string(res->status));
	}

	std::cout << "Before wait" << std::endl;

	view1sub->wait("view1/");
	view2sub->wait("view2/");
	view3sub->wait("view3/");

	if (view1sub->received() != "view1/sub/obj\n" ||
	    view2sub->received() != "view2/sub/obj\n" ||
	    view3sub->received() != "view3/sub/obj\n")
		throw EXCEPTION("Received something unexpected");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		tstnodes nodes(2);

		test1(nodes, "");
		test1(nodes, "sandbox");
		test2(nodes);
		test3(nodes);

		if (hier_exception_flag)
			throw EXCEPTION("Hierarchy node exception was thrown");

	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

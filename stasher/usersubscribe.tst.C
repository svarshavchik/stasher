/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "stasher/client.H"
#include <x/options.H>
#include <x/destroycallbackflag.H>
#include <map>
#include <set>

class test1subscriber
	: public STASHER_NAMESPACE::client::base::subscriberObj {

	std::map<std::string, std::set<std::string> > updateset;
	std::mutex mutex;
	std::condition_variable cond;

public:
	test1subscriber() {}
	~test1subscriber() noexcept {}

	void updated(const std::string &objname,
		     const std::string &suffix)
	{
		std::unique_lock<std::mutex> lock(mutex);

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

	std::cerr << "Subscribing" << std::endl;

	x::ptr<test1subscriber> rootsub=x::ptr<test1subscriber>::create();

	// [SUBSCRIBE]
	STASHER_NAMESPACE::subscriberesultsptr
		rootsubres=cl0->subscribe("", rootsub);

	if (rootsubres->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION("subscribe(\"\") failed");

	x::ptr<test1subscriber> obj1sub=x::ptr<test1subscriber>::create();

	STASHER_NAMESPACE::subscriberesultsptr
		obj1subres=cl0->subscribe("obj1", obj1sub);

	if (obj1subres->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION("subscribe(\"obj1\") failed");

	x::ptr<test1subscriber> obj1hiersub=x::ptr<test1subscriber>::create();

	STASHER_NAMESPACE::subscriberesultsptr
		obj1hiersubres=cl0->subscribe("obj1/", obj1hiersub);

	if (obj1hiersubres->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION("subscribe(\"obj1/\") failed");


	{
		STASHER_NAMESPACE::subscriberesults
			errsubres=cl0->subscribe("err//or",
						x::ptr<test1subscriber>
						::create());

		if (errsubres->status != STASHER_NAMESPACE::req_badname_stat)
			throw EXCEPTION("Managed to subscribed to a bad name");
	}

	{
		STASHER_NAMESPACE::client::base::transaction tran=
			STASHER_NAMESPACE::client::base::transaction::create();

		tran->newobj("obj1", "obj1_value");

		STASHER_NAMESPACE::putresults res=cl1->put(tran);

		if (res->status != STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION(x::tostring(res->status));
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
			throw EXCEPTION(x::tostring(res->status));
	}

	// [USERSUBSCRIBEHIER]
	std::cerr << "Checking callback for obj1: root" << std::endl;
	rootsub->wait("");

	if (obj1sub->received() != "" ||
	    obj1hiersub->received() != "")
		throw EXCEPTION("Unexpected update for obj1/");

	rootsub->clear();

	{
		STASHER_NAMESPACE::client::base::transaction tran=
			STASHER_NAMESPACE::client::base::transaction::create();

		tran->newobj("obj1/2", "obj12_value");

		STASHER_NAMESPACE::putresults res=cl1->put(tran);

		if (res->status != STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION(x::tostring(res->status));
	}

	std::cerr << "Checking callback for obj1/2: obj1" << std::endl;
	obj1hiersub->wait("obj1/");

	if (obj1sub->received() != "" ||
	    rootsub->received() != "")
		throw EXCEPTION("Unexpected update for obj1/2");

	if (obj1hiersub->received() != "obj1/2\n") // [USERSUBSCRIBESUFFIX]
		throw EXCEPTION("Did not received expected update for obj1/2");

	x::destroyCallbackFlag cbroot=x::destroyCallbackFlag::create();
	rootsub->addOnDestroy(cbroot);
	rootsub=x::ptr<test1subscriber>();

	x::destroyCallbackFlag cbobj1=x::destroyCallbackFlag::create();
	obj1sub->addOnDestroy(cbobj1);
	obj1sub=x::ptr<test1subscriber>();

	x::destroyCallbackFlag cbobj1hier=x::destroyCallbackFlag::create();
	obj1hiersub->addOnDestroy(cbobj1hier);
	obj1hiersub=x::ptr<test1subscriber>();

	std::cerr << "Unsubscribing" << std::endl;
	rootsubres=STASHER_NAMESPACE::subscriberesultsptr();
	obj1subres=STASHER_NAMESPACE::subscriberesultsptr();
	obj1hiersubres=STASHER_NAMESPACE::subscriberesultsptr();
	cbroot->wait();
	cbobj1->wait();
	cbobj1hier->wait(); // [UNSUBSCRIBE]
}

static void test2(tstnodes &t)
{
	std::cerr << "test2" << std::endl;
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	STASHER_NAMESPACE::client cl0=
		STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	std::list<STASHER_NAMESPACE::subscriberesults> resList;
	size_t n=0;

	x::ptr<test1subscriber> dummy=x::ptr<test1subscriber>::create();

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
		throw EXCEPTION(x::tostring(res->status));
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		tstnodes nodes(2);

		test1(nodes, "");
		test1(nodes, "sandbox");
		test2(nodes);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

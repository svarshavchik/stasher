/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "trancommit.H"
#include "newtran.H"
#include <x/options.H>
#include <x/destroycallbackflag.H>

static bool cancel_throw_exception=false;

#define DEBUG_DISTRIBUTOR_CANCEL_HOOK() do {				\
		if (cancel_throw_exception)				\
			throw EXCEPTION("Cancelled for testing purposes"); \
	} while (0)

#include "trandistributor.C"

#include "tst.nodes.H"
#include "node.H"
#include <set>

class test1_notifier : public tobjrepoObj::notifierObj {

public:

	test1_notifier() {}
	~test1_notifier() noexcept {}

	std::mutex mutex;
	std::condition_variable cond;

	std::set<std::string> installed_list;
	std::set<std::string> removed_list;

	void installed(const std::string &objname,
		       const x::ptr<x::obj> &lock)

	{
		std::unique_lock<std::mutex> mlock(mutex);

		installed_list.insert(objname);

		cond.notify_all();
	}

	void removed(const std::string &objname,
		     const x::ptr<x::obj> &lock)

	{
		std::unique_lock<std::mutex> mlock(mutex);

		removed_list.insert(objname);
		std::cerr << "R:" << objname << std::endl;
		cond.notify_all();
	}
};

void test1()
{
	x::uuid tranuuid;

	{
		node anode(tstnodes::getnodedir(0));

		anode.start(true);

		cancel_throw_exception=true;

		trandistributorObj::transtatus stat;
		x::ptr<x::obj> mcguffin;
		mcguffin=x::ptr<x::obj>::create();

		{
			newtran tr(anode.repo->newtransaction());

			(*tr->newobj("foo")->getostream())
				<< "bar" << std::flush;

			stat=anode.distributor->newtransaction(tr, mcguffin);
		}

		auto flag=x::destroyCallbackFlag::create();

		mcguffin->ondestroy([flag]{flag->destroyed();});

		mcguffin=nullptr;
		flag->wait();
		anode.wait();
		tranuuid=stat->uuid;
	}

	cancel_throw_exception=false;

	node anode(tstnodes::getnodedir(0));
	x::ptr<test1_notifier> notifier(x::ptr<test1_notifier>::create());

	anode.repo->installNotifier(notifier);

	anode.start(true);

	{
		std::unique_lock<std::mutex> mlock(notifier->mutex);
		bool found=false;

		std::string str="/" + x::tostring(tranuuid);

		while (!found)
		{
			for (std::set<std::string>::iterator
				     b=notifier->removed_list.begin(),
				     e=notifier->removed_list.end();
			     b != e; ++b)
			{
				if (b->substr(0, 4) != "etc/" ||
				    b->size() < str.size())
					continue;

				if (b->substr(b->size() - str.size()) == str)
				{
					found=true;
					break; // [TRANDISTSTARTUPCLEANUP]
				}
			}
			if (found)
				break;

			notifier->cond.wait(mlock);
		}
	}
}

void test2()
{
	x::uuid tranuuid;

	node anode(tstnodes::getnodedir(0));

	{
		newtran tr(anode.repo->newtransaction(tranuuid));

		(*tr->newobj("bar")->getostream())
			<< "bar" << std::flush;

		tr->getOptions()[tobjrepoObj::node_opt]=
			anode.repocluster->getthisnodename();

		tr->finalize();
	}

	x::ptr<test1_notifier> notifier(x::ptr<test1_notifier>::create());

	anode.repo->installNotifier(notifier);

	anode.start(true);

	{
		std::unique_lock<std::mutex> mlock(notifier->mutex);
		bool found=false;

		std::string str="/" + x::tostring(tranuuid);

		while (!found)
		{
			for (std::set<std::string>::iterator
				     b=notifier->removed_list.begin(),
				     e=notifier->removed_list.end();
			     b != e; ++b)
			{
				if (b->substr(0, 4) != "etc/" ||
				    b->size() < str.size())
					continue;

				if (b->substr(b->size() - str.size()) == str)
				{
					found=true;
					break;
				}
			}
			if (found)
				break;

			notifier->cond.wait(mlock);
		}
	}
}

void test3()
{
	std::cerr << "The following errors are expected:" << std::endl;

	node anode(tstnodes::getnodedir(0));
	node bnode(tstnodes::getnodedir(1));

        {
                STASHER_NAMESPACE::nodeinfomap clusterinfo;

                STASHER_NAMESPACE::nodeinfo info;

                info.options.insert(std::make_pair(STASHER_NAMESPACE::nodeinfo::host_option,
                                                   "localhost"));

                clusterinfo[anode.listener->nodeName()]=info;

		info.nomaster(true);
                clusterinfo[bnode.listener->nodeName()]=info;

                repoclusterinfoObj::saveclusterinfo(anode.repo, clusterinfo);
                repoclusterinfoObj::saveclusterinfo(bnode.repo, clusterinfo);
        }

	{
		x::uuid tuuid;

		newtran tr=anode.repo->newtransaction(tuuid);

		tr->newobj("foo");
		tr->newobj("bar");

		tr->getOptions()[tobjrepoObj::node_opt]=
			anode.repocluster->getthisnodename();

		tr->finalize();
		anode.repo->begin_commit(tuuid, x::eventfd::create())->commit();
	}

	anode.start(true);
	bnode.start(true);
	anode.debugWaitFullQuorumStatus(false);
	bnode.debugWaitFullQuorumStatus(false);
	anode.listener->connectpeers();
	anode.debugWaitFullQuorumStatus(true);
	bnode.debugWaitFullQuorumStatus(true);

	tobjrepoObj::values_t valuesMap;
	std::set<std::string> notfound;
	std::set<std::string> s;

	s.insert("foo");
	s.insert("bar");

	anode.repo->values(s, false, valuesMap, notfound);

	if (valuesMap.find("foo") == valuesMap.end())
		throw EXCEPTION("test2 failed");

	if (valuesMap.find("bar") == valuesMap.end())
		throw EXCEPTION("test3 failed");

	x::uuid foouuid=valuesMap["foo"].first;
	x::uuid baruuid=valuesMap["bar"].first;

	spacemonitor adf=anode.listener->df(),
		bdf=bnode.listener->df();

	uint64_t freeinode=adf->freeinodes();

	spacemonitorObj::reservation r1=adf->reservespace_alloc(0, freeinode),
		r2=adf->reservespace_alloc(0, freeinode),
		r3=bdf->reservespace_alloc(0, freeinode),
		r4=bdf->reservespace_alloc(0, freeinode);

	if (!adf->outofspace() || !bdf->outofspace())
		throw EXCEPTION("test3: failed to simulate an out of space condition");

	trandistributorObj::transtatus stat;
	x::ptr<x::obj> mcguffin;

	mcguffin=x::ptr<x::obj>::create();

	{
		newtran tr(anode.repo->newtransaction());

		(*tr->newobj("test2p1")->getostream())
			<< "baz" << std::flush;

		stat=anode.distributor->newtransaction(tr, mcguffin);

		auto flag=x::destroyCallbackFlag::create();
		mcguffin->ondestroy([flag]{flag->destroyed();});
		mcguffin=nullptr;
		flag->wait();
	}

	if (stat->status != STASHER_NAMESPACE::req_failed_stat)
		throw EXCEPTION("Transaction did not fail in no space condition");

	mcguffin=x::ptr<x::obj>::create();

	{
		newtran tr(bnode.repo->newtransaction());

		(*tr->newobj("test2p2")->getostream())
			<< "baz" << std::flush;

		stat=bnode.distributor->newtransaction(tr, mcguffin);

		auto flag=x::destroyCallbackFlag::create();
		mcguffin->ondestroy([flag]{flag->destroyed();});
		mcguffin=nullptr;
		flag->wait();
	}

	if (stat->status != STASHER_NAMESPACE::req_failed_stat)
		throw EXCEPTION("Transaction did not fail in no space condition");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		tstnodes nodes(2);

		std::cout << "test1" << std::endl;
		test1();

		std::cout << "test2" << std::endl;
		test2();

		std::cout << "test3" << std::endl;
		test3();

	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

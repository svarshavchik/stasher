/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "stasher/client.H"
#include "stasher/manager.H"
#include "stasher/managedsubscriber.H"
#include "stasher/puttransaction.H"
#include <x/options.H>

class test1subscriber : public STASHER_NAMESPACE::managedsubscriberObj {

public:
	std::set<std::string> updateset;
	STASHER_NAMESPACE::req_stat_t stat;
	bool stat_received;
	std::mutex mutex;
	std::condition_variable cond;

	test1subscriber() : stat_received(false) {}
	~test1subscriber() {}

	void updated(const std::string &objname, const std::string &suffix)
		override
	{
		std::unique_lock<std::mutex> lock(mutex);

		updateset.insert(objname + suffix);
		cond.notify_all();
	}

	void connection_update(STASHER_NAMESPACE::req_stat_t statArg)
		override
	{
		std::unique_lock<std::mutex> lock(mutex);

		stat_received=true;
		stat=statArg;
		cond.notify_all();
	}
};

static void test1(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	auto client=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	auto manager=STASHER_NAMESPACE::manager::create("", "2 seconds");

	std::cerr << "Subscribing" << std::endl;

	x::ptr<test1subscriber> rootsub=x::ptr<test1subscriber>::create();

	auto mcguffin=manager->manage_subscription(client, "", rootsub);

	{
		std::unique_lock<std::mutex> lock(rootsub->mutex);

		rootsub->cond.wait(lock, [&rootsub] {
				return rootsub->stat_received;
			}); // [MANAGEDSUBINIT]

		if (rootsub->stat != STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION("Subscription request failed with "
					<< x::to_string(rootsub->stat));
	}

	{
		auto put=STASHER_NAMESPACE::client::base::transaction::create();

		put->newobj("a", "a");

		auto res=client->put(put);

		if (res->status != STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION("PUT failed");


		std::unique_lock<std::mutex> lock(rootsub->mutex);

		rootsub->cond.wait(lock, [&rootsub]
				   { return !rootsub->updateset.empty(); });

		if (rootsub->updateset.size() != 1 ||
		    *rootsub->updateset.begin() != "a")
			// {MANAGEDSUBUPD]
		{
			throw EXCEPTION("[MANAGEDSUBUPD] failed");
		}
	}

	{
		std::unique_lock<std::mutex> lock(rootsub->mutex);

		rootsub->stat_received=false;

		std::cerr << "Stopping the server" << std::endl;
		tnodes.clear();

		rootsub->cond.wait(lock, [&rootsub] {
				return rootsub->stat_received;
			}); // [MANAGEDSUBINIT]

		if (rootsub->stat != STASHER_NAMESPACE::req_disconnected_stat)
			throw EXCEPTION("Expected to get a disconnected status, but got a "
					<< x::to_string(rootsub->stat)
					<< " instead");
	}
}

static void test2(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	auto client=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	auto manager=STASHER_NAMESPACE::manager::create();

	std::cerr << "Subscribing" << std::endl;

	x::ptr<test1subscriber> rootsub=x::ptr<test1subscriber>::create();

	auto mcguffin=manager->manage_subscription(client, "", rootsub);

	{
		std::unique_lock<std::mutex> lock(rootsub->mutex);

		rootsub->cond.wait(lock, [&rootsub] {
				return rootsub->stat_received;
			}); // [MANAGEDSUBINIT]

		if (rootsub->stat != STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION("Subscription request failed with "
					<< x::to_string(rootsub->stat));
	}

	{
		std::unique_lock<std::mutex> lock(rootsub->mutex);

		rootsub->stat_received=false;

		std::cerr << "Stopping the server" << std::endl;
		tnodes.clear();

		sleep(2);
	}
}

static void test3(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	auto client=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));
	auto manager=STASHER_NAMESPACE::manager::create();

	x::ref<test1subscriber> rootsub=x::ref<test1subscriber>::create();

	auto mcguffin=manager->manage_subscription(client, "foo//bar", rootsub);

	{
		std::unique_lock<std::mutex> lock(rootsub->mutex);

		rootsub->cond.wait(lock, [&rootsub] {
				return rootsub->stat_received;
				});

		if (rootsub->stat != STASHER_NAMESPACE::req_badname_stat)
			throw EXCEPTION("Bad name wasn't rejected");
	}
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		tstnodes nodes(1);

		test1(nodes);
		x::property::load_property("objrepo::manager", "5 minutes",
					   true, true);
		std::cerr << "test2" << std::endl;
		test2(nodes);

		std::cerr << "test3" << std::endl;
		test3(nodes);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

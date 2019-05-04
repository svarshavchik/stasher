/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "stasher/client.H"
#include "stasher/manager.H"
#include "stasher/managedserverstatuscallback.H"
#include <x/options.H>

class test1subscriber
	: public STASHER_NAMESPACE::managedserverstatuscallbackObj {

public:
	STASHER_NAMESPACE::clusterstate clusterstate;
	bool state_received;
	STASHER_NAMESPACE::req_stat_t stat;
	bool status_received;
	std::mutex mutex;
	std::condition_variable cond;

	test1subscriber() : state_received(false), status_received(false) {}
	~test1subscriber() {}

	void serverinfo(const STASHER_NAMESPACE::userhelo &serverinfo)
		override
	{
	}

	void state(const STASHER_NAMESPACE::clusterstate &state)
		override
	{
		std::unique_lock<std::mutex> lock(mutex);

		clusterstate=state;
		state_received=true;
		cond.notify_all();
	}

	void connection_update(STASHER_NAMESPACE::req_stat_t statArg)
		override
	{
		std::unique_lock<std::mutex> lock(mutex);

		status_received=true;
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

	auto manager=STASHER_NAMESPACE::manager::create();

	std::cerr << "Subscribing" << std::endl;

	x::ptr<test1subscriber> rootsub=x::ptr<test1subscriber>::create();

	auto mcguffin=manager->manage_serverstatusupdates(client, rootsub);

	{
		std::unique_lock<std::mutex> lock(rootsub->mutex);

		rootsub->cond.wait(lock, [&rootsub] {
				return rootsub->status_received &&
					rootsub->state_received &&
					rootsub->clusterstate.full;
			});

		if (rootsub->stat != STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION("Subscription request failed with "
					<< x::to_string(rootsub->stat));
	}
	std::cerr << "Dropping another node" << std::endl;

	tnodes[2]=tstnodes::noderef();

	{
		std::unique_lock<std::mutex> lock(rootsub->mutex);

		rootsub->cond.wait(lock, [&rootsub] {
				return !rootsub->clusterstate.full;
			});
	}

	std::cerr << "Dropping this node" << std::endl;
	tnodes[0]=tstnodes::noderef();

	{
		std::unique_lock<std::mutex> lock(rootsub->mutex);

		rootsub->cond.wait(lock, [&rootsub] {
				return rootsub->stat ==
					STASHER_NAMESPACE
					::req_disconnected_stat;
				// [MANAGEDSERVERSTATUSCONN]
			});

		rootsub->state_received=false;
	}

	std::cerr << "Reconnecting" << std::endl;

	tnodes[0]=tstnodes::noderef::create(tstnodes::getnodedir(0));
	tnodes[0]->start(false);

	{
		std::unique_lock<std::mutex> lock(rootsub->mutex);

		rootsub->cond.wait(lock, [&rootsub] {
				return rootsub->state_received &&
					rootsub->stat
					== STASHER_NAMESPACE
					::req_processed_stat;

				// [MANAGEDSERVERSTATUSCONN]
			});

		if (rootsub->clusterstate.full)
			throw EXCEPTION("Where does the full majority come from?");
	}

	std::cerr << "Waiting for full quorum" << std::endl;
	tnodes[2]=tstnodes::noderef::create(tstnodes::getnodedir(2));
	tnodes[2]->start(false);

	{
		std::unique_lock<std::mutex> lock(rootsub->mutex);

		rootsub->cond.wait(lock, [&rootsub] {
				return rootsub->clusterstate.full;
			});
		// [MANAGEDSERVERSTATUSQUORUM]
	}

}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		tstnodes nodes(3);

		x::property::load_property("objrepo::manager", "2 seconds",
					   true, true);
		x::property::load_property("reconnect", "4", true, true);

		std::cerr << "test1" << std::endl;

		test1(nodes);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

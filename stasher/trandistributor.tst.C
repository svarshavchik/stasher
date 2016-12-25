/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "trandistributor.H"
#include "repoclusterinfo.H"
#include "repocontrollerslave.H"
#include "repocontrollermaster.H"
#include "tobjrepo.H"
#include "newtran.H"
#include "peerstatus.H"
#include "stoppablethreadtracker.H"
#include "tranreceived.H"
#include "trandistreceived.H"
#include "trandistuuid.H"
#include "trandistributor.H"
#include <x/options.H>

static const char repo1[]="conftest1.dir";

class dummyhalt : public x::stoppableObj {

public:
	dummyhalt()=default;
	~dummyhalt() noexcept=default;

	void stop() override
	{
	}
};

class myclusterinfo : public repoclusterinfoObj, public x::stoppableObj {

	STASHER_NAMESPACE::stoppableThreadTracker tracker;

public:
	myclusterinfo(const std::string &nodename,
		      const tobjrepo &repoArg,
		      const STASHER_NAMESPACE::stoppableThreadTracker &trackerArg)
		: repoclusterinfoObj(nodename, "test",
				     trackerArg, repoArg),
		  tracker(trackerArg)
	{
	}

	~myclusterinfo() noexcept {}

	repocontroller_start_info
	create_master_controller(const std::string &mastername,
				 const x::uuid &masteruuid,
				 const tobjrepo &repo,
				 const repoclusterquorum &callback_listArg)
		override
	{
		auto controller=repocontrollermaster
			::create(mastername, masteruuid,
				 repo,
				 callback_listArg,
				 x::ptr<trandistributorObj>(),
				 tracker, x::ref<dummyhalt>::create());

		return repocontroller_start_info::create(controller);
	}

	repocontroller_start_info
	create_slave_controller(const std::string &mastername,
				const x::ptr<peerstatusObj> &peer,
				const x::uuid &masteruuid,
				const tobjrepo &repo,
				const repoclusterquorum &callback_listArg)
		override
	{
		auto controller=x::ref<repocontrollerslaveObj>
			::create(mastername, peer,
				 masteruuid, repo,
				 callback_listArg,
				 x::ptr<trandistributorObj>(), tracker,
				 x::ref<dummyhalt>::create());

		return repocontroller_start_info::create(controller);
	}

	void stop() override
	{
	}
};

class mydistributor : public trandistributorObj {

public:
	mydistributor() : flag(false) {}
	~mydistributor() noexcept {}

	bool flag;
	std::mutex mutex;
	std::condition_variable cond;

	void do_dispatch_cluster_update(const clusterinfoObj::cluster_t &newStatus) override

	{
		trandistributorObj::do_dispatch_cluster_update(newStatus);

		std::unique_lock<std::mutex> lock(mutex);

		flag=true;
		cond.notify_all();
	}

	void wait()
	{
		std::unique_lock<std::mutex> lock(mutex);

		while (!flag)
			cond.wait(lock);
		flag=false;
	}
};

class enumeratecb : public tobjrepoObj::finalized_cb {

public:
	std::set<x::uuid> uuids;

	enumeratecb() {}
	~enumeratecb() noexcept {}

	void operator()(const x::uuid &uuid,
			const dist_received_status_t &status)

	{
		uuids.insert(uuid);
	}
};


class mytranreceivedObj : public trandistreceivedObj {

public:
	mytranreceivedObj() {}
	~mytranreceivedObj() noexcept {}

	std::mutex mutex;
	std::condition_variable cond;

	std::set<x::uuid> received_uuids, cancelled_uuids;

	void received(const trandistuuid &uuids)
	{
		std::unique_lock<std::mutex> lock(mutex);

		for (std::map<x::uuid, dist_received_status_t>::iterator
			     b=uuids->uuids.begin(),
			     e=uuids->uuids.end(); b != e; ++b)
			received_uuids.insert(b->first);
		cond.notify_all();
	}

	void cancelled(const tranuuid &uuids)
	{
		std::unique_lock<std::mutex> lock(mutex);

		cancelled_uuids.insert(uuids->uuids.begin(),
				       uuids->uuids.end());
		cond.notify_all();
	}
};

static void test1() // [PURGETRANSOURCEUNKNOWN].
{
	STASHER_NAMESPACE::stoppableThreadTrackerImpl
		tracker(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create());

	tobjrepo repo(tobjrepo::create(repo1));

	{
		STASHER_NAMESPACE::nodeinfomap cluster;

		cluster["node1"]=STASHER_NAMESPACE::nodeinfo();
		cluster["node2"]=STASHER_NAMESPACE::nodeinfo();

		myclusterinfo::saveclusterinfo(repo, cluster);
	}

	auto cluster=x::ref<myclusterinfo>::create("node1", repo,
						   tracker->getTracker());

	x::ref<mydistributor> distributor(x::ref<mydistributor>::create());

	auto msgqueue = trandistributorObj::msgqueue_obj::create(distributor);

	cluster->initialize();

	x::uuid node1tran;
	x::uuid node2tran;
	x::uuid node3tran;

	{
		newtran tr(repo->newtransaction());

		tr->getOptions()[tobjrepoObj::node_opt]="node1";

		node1tran=tr->finalize();
	}

	std::cout << "node 1 transaction: " << x::tostring(node1tran)
		  << std::endl;

	{
		newtran tr(repo->newtransaction());

		tr->getOptions()[tobjrepoObj::node_opt]="node2";

		node2tran=tr->finalize();
	}

	{
		newtran tr(repo->newtransaction());

		tr->getOptions()[tobjrepoObj::node_opt]="node3";

		node3tran=tr->finalize();
	}

	tracker->start_thread(distributor,
			      msgqueue,
			      cluster, repo, x::ptr<x::obj>());
	distributor->wait();

	std::cout << distributor->debugGetReport()->report << std::endl;

	enumeratecb uuids;

	repo->enumerate(uuids);

	if (uuids.uuids.size() != 2 ||
	    uuids.uuids.find(node1tran) == uuids.uuids.end() ||
	    uuids.uuids.find(node2tran) == uuids.uuids.end())
		throw EXCEPTION("Distributor initialization failure");


	{
		std::map<std::string, nodeinfoconn> cluster;

		cluster["node1"]=nodeinfoconn();

		distributor->clusterupdated(cluster);
	}

	distributor->wait();

	uuids.uuids.clear();
	repo->enumerate(uuids);

	if (uuids.uuids.size() != 1 ||
	    uuids.uuids.find(node1tran) == uuids.uuids.end())
		throw EXCEPTION("Distributor update failure");

	x::ptr<mytranreceivedObj> tr(x::ptr<mytranreceivedObj>::create());

	distributor->installreceiver(tr);

	{
		std::unique_lock<std::mutex> lock(tr->mutex);

		// [TRANDISTRECVINSTALL]
		while (tr->received_uuids.size() != 1 ||
		       tr->received_uuids.find(node1tran) ==
		       tr->received_uuids.end())
			tr->cond.wait(lock);
		tr->received_uuids.clear();

	}

	// [TRANDISTRECVREMOVE]

	distributor->canceltransaction(node1tran);

	{
		std::unique_lock<std::mutex> lock(tr->mutex);

		while (tr->cancelled_uuids.size() != 1 ||
		       tr->cancelled_uuids.find(node1tran) ==
		       tr->cancelled_uuids.end())
			tr->cond.wait(lock);
		tr->cancelled_uuids.clear();

	}

	node1tran=x::uuid();

	distributor->newtransaction(repo->newtransaction(node1tran),
				    x::ptr<x::obj>());

	// [TRANDISTRECVADD]
	{
		std::unique_lock<std::mutex> lock(tr->mutex);

		while (tr->received_uuids.size() != 1 ||
		       tr->received_uuids.find(node1tran) ==
		       tr->received_uuids.end())
			tr->cond.wait(lock);
		tr->received_uuids.clear();

	}
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		x::dir::base::rmrf(repo1);

		ALARM(60);

		std::cout << "test1" << std::endl;
		test1();

		x::dir::base::rmrf(repo1);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

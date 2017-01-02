/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "repoclusterinfo.H"
#include "repocontrollerbase.H"
#include "repopeerconnection.H"
#include "peerstatus.H"
#include "tobjrepo.H"
#include <x/options.H>
#include <x/stoppable.H>

class mycontroller : public repocontrollerbaseObj,
		     public x::threadmsgdispatcherObj {

public:

	using repocontrollerbaseObj::quorum;
	using repocontrollerbaseObj::started;

	static int instance_counter, mcguffin_counter, total_instance_counter;
	static x::ptr<x::obj> *objrefptr;

	mycontroller(const std::string &masternameArg,
		     const x::uuid &masteruuidArg,
		     const tobjrepo &repoArg,
		     const repoclusterquorum &callback_listArg)
		: repocontrollerbaseObj(masternameArg,
					masteruuidArg,
					repoArg,
					callback_listArg)
	{
		++instance_counter;
		++total_instance_counter;
	}

	~mycontroller()
	{
		--instance_counter;
	}

	x::ptr<x::obj> mcguffin;

	void get_quorum(const STASHER_NAMESPACE::quorumstateref &status_arg,
			const boolref &processed_arg,
			const x::ptr<x::obj> &mcguffin_arg)
	{
		static_cast<STASHER_NAMESPACE::quorumstate &>(*status_arg)=
			STASHER_NAMESPACE::quorumstate();
		processed_arg->flag=true;
	}

	start_controller_ret_t
	start_controller(const x::threadmsgdispatcherObj::msgqueue_obj &msgqueue,
			 const x::ref<x::obj> &mcguffinArg)
	{
		mcguffin=mcguffinArg;
		++mcguffin_counter;
		objrefptr= &mcguffin;
		return start_controller_ret_t::create();
	}

	void handoff(const repocontroller_start_info &next)
	{
		x::ptr<x::obj> obj(mcguffin);

		mcguffin=nullptr;
		--mcguffin_counter;

		next->start(obj);
	}

	void peernewmaster(const repopeerconnectionptr &peerRef,
			   const nodeclusterstatus &peerStatus)

	{
	}

	x::ptr<x::obj>
	handoff_request(const std::string &peername)
	{
		return x::ptr<x::obj>();
	}

	void halt(const STASHER_NAMESPACE::haltrequestresults &req,
		  const x::ref<x::obj> &mcguffin)
	{
	}
};

int mycontroller::instance_counter, mycontroller::mcguffin_counter,
	mycontroller::total_instance_counter;

x::ptr<x::obj> *mycontroller::objrefptr;

class myrepoclusterinfoObj :
	public STASHER_NAMESPACE::stoppableThreadTrackerImpl,
	public repoclusterinfoObj, public x::stoppableObj {

public:
	int master_cnt, slave_cnt;
	bool stop_received;

	x::ptr<mycontroller> mastercontroller;

	myrepoclusterinfoObj(const std::string &nodename,
			     const tobjrepo &repoArg)
		: STASHER_NAMESPACE::stoppableThreadTrackerImpl(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create()),
		  repoclusterinfoObj(nodename, "test",
				     STASHER_NAMESPACE::stoppableThreadTrackerImpl::
				     operator->()->getTracker(), repoArg),
		  master_cnt(0), slave_cnt(0), stop_received(false)
	{
	}

	~myrepoclusterinfoObj()
	{
	}

	void stop_threads(bool permanently=true)
	{
		STASHER_NAMESPACE::stoppableThreadTrackerImpl
			::operator->()->stop_threads(permanently);
	}

	repocontroller_start_info
	create_master_controller(const std::string &mastername,
				 const x::uuid &masteruuid,
				 const tobjrepo &repo,
				 const repoclusterquorum &callback_listArg)

	{
		++master_cnt;

		auto p=x::ref<mycontroller>
			::create(mastername, masteruuid, repo,
				 callback_listArg);
		mastercontroller=p;
		return repocontroller_start_info::create(p);
	}

	repocontroller_start_info
	create_slave_controller(const std::string &mastername,
				const x::ptr<peerstatusObj> &peer,
				const x::uuid &masteruuid,
				const tobjrepo &repo,
				const repoclusterquorum &callback_listArg)

	{
		++slave_cnt;

		auto p=x::ref<mycontroller>::create(mastername, masteruuid,
						    repo,
						    callback_listArg);

		return repocontroller_start_info::create(p);
	}

	void stop()
	{
		stop_received=true;
	}
};

class dummypeerstatusObj : public repopeerconnectionObj {

public:
	using peerstatusObj::peerstatusupdate;
	using peerstatusObj::install;

	dummypeerstatusObj(const std::string &peername)
		: repopeerconnectionObj(peername,
					spacemonitor::create(x::df::create(".")
							     )) {}

	~dummypeerstatusObj() {}
};

typedef x::ptr<dummypeerstatusObj> dummypeerstatus;

static void test1()
{
	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	STASHER_NAMESPACE::nodeinfomap clusterinfo;

	clusterinfo["nodeb"]=STASHER_NAMESPACE::nodeinfo();

	repoclusterinfoObj::saveclusterinfo(repo, clusterinfo);

	auto clust=x::ref<myrepoclusterinfoObj>::create("nodea", repo);

	// [INITIALCONTROLLER]
	clust->initialize();

	{
		STASHER_NAMESPACE::nodeinfo dummy;

		clust->getnodeinfo("nodeb", dummy); // [INITCONFIG]
	}

	clusterinfo["nodec"]=STASHER_NAMESPACE::nodeinfo();
	repoclusterinfoObj::saveclusterinfo(repo, clusterinfo);

	{
		STASHER_NAMESPACE::nodeinfo dummy;

		clust->getnodeinfo("nodec", dummy);
	}

	if (clust->master_cnt != 1 || clust->slave_cnt != 0)
		throw EXCEPTION("[INITIALCONTROLLER] failed");

	auto nodeb=dummypeerstatus::create("nodeb"),
		nodec=dummypeerstatus::create("nodec");

	x::uuid masteruuid;

	nodeb->peerstatusupdate(nodeclusterstatus("nodec", masteruuid, 1,
						  false));
	nodec->peerstatusupdate(nodeclusterstatus("nodec", masteruuid, 1,
						  false));

	// [CONTROLLER]
	if (!nodeb->install(clust).first ||
	    clust->master_cnt != 1 || clust->slave_cnt != 0)
		throw EXCEPTION("[CONTROLLER] test 1 failed");

	if (!nodec->install(clust).first ||
	    clust->master_cnt != 1 || clust->slave_cnt != 1)
		throw EXCEPTION("[CONTROLLER] test 2 failed");

	nodeb=dummypeerstatus();
	clust->stop_threads(false);

	nodec->peerstatusupdate(nodeclusterstatus("nodec", masteruuid, 0,
						  false));
	if (clust->master_cnt != 1 || clust->slave_cnt != 1)
		throw EXCEPTION("[CONTROLLER] test 3 failed");

	nodec=dummypeerstatus();
	clust->stop_threads(false);

	if (clust->master_cnt != 2 || clust->slave_cnt != 1)
		throw EXCEPTION("[CONTROLLER] test 4 failed");

	if (mycontroller::instance_counter != 1 ||
	    mycontroller::mcguffin_counter != 1 ||
	    mycontroller::total_instance_counter != 3)
		throw EXCEPTION("[CONTROLLER] test 5 failed");

	// [ABORT]
	if (clust->stop_received)
		throw EXCEPTION("[ABORT] test 1 failed");

	*mycontroller::objrefptr=nullptr;
	clust->stop_threads(false);

	if (!clust->stop_received)
		throw EXCEPTION("[ABORT] test 2 failed");
}

class test2cb : public STASHER_NAMESPACE::repoclusterquorumcallbackbaseObj {

public:
	bool flag;
	std::mutex mutex;
	std::condition_variable cond;

	test2cb() : flag(false) {}
	~test2cb() {}

	void quorum(const STASHER_NAMESPACE::quorumstate &state)

	{
		if (state.full)
		{
			std::lock_guard<std::mutex> lock(mutex);

			flag=true;
			cond.notify_all();
		}
	}
};

static void test2()
{
	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	auto clust=x::ref<myrepoclusterinfoObj>::create("nodea", repo);

	clust->initialize();

	x::ptr<test2cb> cb(x::ptr<test2cb>::create());

	clust->installQuorumNotification(cb); // [QUORUMCB]
	clust->mastercontroller->started();
	clust->mastercontroller->quorum(STASHER_NAMESPACE::quorumstate(true,
								       true));

	std::unique_lock<std::mutex> lock(cb->mutex);

	while (!cb->flag)
		cb->cond.wait(lock);
}

class test3statusObj : public peerstatusObj::adapterObj {

public:
	test3statusObj(const std::string &peername)
		: peerstatusObj::adapterObj(peername) {}
	~test3statusObj() {}

	using peerstatusObj::peerstatusupdate;
	using peerstatusObj::install;
};

static void test3()
{
	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	x::uuid nodeauuid=repo->newtransaction()->finalize(),
		nodebuuid=repo->newtransaction()->finalize();

	repo->mark_done(nodeauuid, "nodea", STASHER_NAMESPACE::req_processed_stat,
			x::ptr<x::obj>());
	repo->mark_done(nodebuuid, "nodeb", STASHER_NAMESPACE::req_processed_stat,
			x::ptr<x::obj>());

	STASHER_NAMESPACE::nodeinfomap clusterinfo;

	clusterinfo["nodeb"]=STASHER_NAMESPACE::nodeinfo();

	repoclusterinfoObj::saveclusterinfo(repo, clusterinfo);

	auto clust=x::ref<myrepoclusterinfoObj>::create("nodea", repo);

	if (repo->get_tran_stat("nodea", nodeauuid) != // [INITREMOVEDONE]
	    STASHER_NAMESPACE::req_failed_stat ||
	    repo->get_tran_stat("nodeb", nodebuuid) !=
	    STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION("[INITREMOVEDONE] failed");

	clust->initialize();

	auto peer=x::ref<test3statusObj>::create("nodeb");

	peer->peerstatusupdate(nodeclusterstatus("nodec", x::uuid(), 0, false));

	if (!peer->install(clust).first)
		throw EXCEPTION("[FALSEMASTER] failed");

	peer=x::ptr<test3statusObj>::create("nodeb");
	clust->stop_threads(false);

	std::cerr << "The following error is expected:" << std::endl;

	peer->peerstatusupdate(nodeclusterstatus("nodea", x::uuid(), 0, false));

	if (peer->install(clust).first)
		throw EXCEPTION("[FALSEMASTER] failed");
}

static void test4()
{
	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	STASHER_NAMESPACE::nodeinfomap clusterinfo;

	clusterinfo["a"]=STASHER_NAMESPACE::nodeinfo();
	clusterinfo["b"]=STASHER_NAMESPACE::nodeinfo();
	clusterinfo["c"]=STASHER_NAMESPACE::nodeinfo();

	{
		std::vector<STASHER_NAMESPACE::nodeinfomap::iterator> ovec;

		ovec.push_back(clusterinfo.find("c"));
		ovec.push_back(clusterinfo.find("b"));
		ovec.push_back(clusterinfo.find("a"));

		STASHER_NAMESPACE::nodeinfo::savenodeorder(ovec);
	}

	repoclusterinfoObj::saveclusterinfo(repo, clusterinfo);
	auto cluster=x::ref<myrepoclusterinfoObj>::create("a", repo);

	clusterinfo.clear();
	cluster->get(clusterinfo);
	std::vector<STASHER_NAMESPACE::nodeinfomap::iterator> ovec;
	STASHER_NAMESPACE::nodeinfo::loadnodeorder(clusterinfo, ovec);

	// [NODEORDER]
	if (ovec.size() != 3 || ovec[0]->first != "c" ||
	    ovec[1]->first != "b" || ovec[2]->first != "a")
		throw EXCEPTION("[NODEORDER] failed");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	ALARM(30);

	try {
		std::cout << "test1" << std::endl;
		x::dir::base::rmrf("conftestdir.tst");
		test1();
		std::cout << "test2" << std::endl;
		x::dir::base::rmrf("conftestdir.tst");
		test2();
		std::cout << "test3" << std::endl;
		test3();
		std::cout << "test4" << std::endl;
		test4();
		x::dir::base::rmrf("conftestdir.tst");
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

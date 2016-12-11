/*
** Copyright 2012-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"

#define DEBUG_DISABLE_MASTER_COMMITS
#include "repocontrollermaster.C"

#include "tranrecvcanc.H"

static void (*debug_recv_trans)(const std::string &,
				const tranrecvcanc &)=NULL;

#define DEBUG_RECEIVED_TRANS(msg) do {					\
		if (debug_recv_trans) (*debug_recv_trans)(peername,	\
							  msg); } while (0)

static int debug_ping_cnt=0;

#define DEBUG_PINGPONG() (++debug_ping_cnt)

#include "repopeerconnection.C"

#include "repoclusterinfoimpl.H"
#include "clusterlistenerimpl.H"
#include "stoppablethreadtracker.H"
#include "clustertlsconnectshutdown.H"
#include "trandistributor.H"

#include "tranreceived.H"
#include "trandistreceived.H"

#include "repomg.H"
#include "newtran.H"
#include "trancommit.H"

#include "trandistcancel.H"
#include "transerializer.H"
#include <x/options.H>


class quorumcbObj: public STASHER_NAMESPACE::repoclusterquorumcallbackbaseObj {

public:
	std::mutex mutex;
	std::condition_variable cond;
	bool flag;
	bool quorum_received;

	quorumcbObj() : flag(false), quorum_received(false)
	{
	}

	~quorumcbObj() noexcept
	{
	}

	void quorum(const STASHER_NAMESPACE::quorumstate &inquorum)

	{
		std::unique_lock<std::mutex> lock(mutex);

		flag=inquorum.full;
		quorum_received=true;
		cond.notify_all();
	}

	void wait4(bool forwhat)
	{
		std::unique_lock<std::mutex> lock(mutex);

		while (quorum_received == false || flag != forwhat)
			cond.wait(lock);
	}
};

class mydistributor : public trandistributorObj {

public:
	mydistributor() {}
	~mydistributor() noexcept {}

	std::mutex mutex;
	std::condition_variable cond;

	std::set<x::uuid> deserialized;
	std::set<x::uuid> cancelled;

	void dispatch_deserialized_cancel(const trandistcancel &cancel)
		override
	{
		do_dispatch_deserialized_cancel(cancel);

		std::unique_lock<std::mutex> lock(mutex);

		cancelled.insert(cancel.uuids.begin(),
				 cancel.uuids.end());
		cond.notify_all();
	}

	void dispatch_deserialized_transaction(const newtran &tran,
					       const x::uuid &uuid)
		override
	{
		do_dispatch_deserialized_transaction(tran, uuid);

		std::unique_lock<std::mutex> lock(mutex);

		std::cerr << "Deserialized "
			  << x::tostring(uuid)
			  << std::endl;

		deserialized.insert(uuid);
		cond.notify_all();
	}
};

static const char clustername[]="test";
static const char clusterdir[]="conftestcluster1.dir";
static const char nodea[]="nodea";
static const char nodeb[]="nodeb";
static const char nodeadir[]="conftestnodea.dir";
static const char nodebdir[]="conftestnodeb.dir";

class instance {

public:
	tobjrepo repo;
	STASHER_NAMESPACE::stoppableThreadTrackerImpl trackerimpl;
	STASHER_NAMESPACE::stoppableThreadTracker tracker;
	clusterlistenerimpl listener;
	x::ptr<mydistributor> distributor;
	repoclusterinfoimpl repocluster;
	x::ptr<quorumcbObj> quorumstatus;

	instance(const std::string &dir)
		: repo(tobjrepo::create(dir)),
		  trackerimpl(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create()),
		  tracker(trackerimpl->getTracker()),
		  listener(clusterlistenerimpl::create(dir)),
		  distributor(x::ptr<mydistributor>::create()),
		  repocluster(repoclusterinfoimpl
			      ::create(listener->nodeName(),
				       listener->clusterName(), repo,
				       distributor, tracker)),
		  quorumstatus(x::ptr<quorumcbObj>::create())
	{
	}

	void start()
	{
		auto msgqueue=mydistributor::msgqueue_obj::create(distributor);

		repocluster->initialize();
		tracker->start_thread(x::ref<mydistributor>(distributor),
				      msgqueue,
				      repocluster, repo,
				      x::ptr<x::obj>::create());

		tracker->start_thread(listener,
				      tracker,
				      distributor,
				      repo,
				      x::ptr<clustertlsconnectshutdownObj>
				      ::create(),
				      repocluster);
		repocluster->installQuorumNotification(quorumstatus);
		(void)repocluster->debug_inquorum();
	}

	~instance() noexcept
	{
	}

	void waitquorumstatus(bool flag)
	{
		while (repocluster->debug_inquorum()->full != flag)
		{
			quorumstatus->wait4(flag);
		}
	}
};

void test1()
{
	instance a(nodeadir);
	instance b(nodebdir);

	std::cerr << "Instances contructed" << std::endl;

	{
		STASHER_NAMESPACE::nodeinfomap clusterinfo;

		STASHER_NAMESPACE::nodeinfo info;

		info.options.insert(std::make_pair(STASHER_NAMESPACE::nodeinfo::host_option,
						   "localhost"));

		clusterinfo[a.listener->nodeName()]=info;
		clusterinfo[b.listener->nodeName()]=info;

		repoclusterinfoObj::saveclusterinfo(a.repo, clusterinfo);
		repoclusterinfoObj::saveclusterinfo(b.repo, clusterinfo);
	}

	a.start();
	a.waitquorumstatus(false);
	b.start();

	std::cout << "a quorum: "
		  << x::tostring(*a.repocluster->debug_inquorum()) << std::endl;
	std::cout << "b quorum: "
		  << x::tostring(*b.repocluster->debug_inquorum()) << std::endl;

	a.listener->connectpeers();

	a.waitquorumstatus(true);
	b.waitquorumstatus(true);
	std::cerr << "cluster is now in quorum" << std::endl;

	x::uuid tran1;

	a.distributor->newtransaction(a.repo->newtransaction(tran1),
				      x::ptr<x::obj>());

	std::cerr << "Waiting for " << x::tostring(tran1)
		  << " to be deserialized" << std::endl;

	{
		std::unique_lock<std::mutex> lock(b.distributor->mutex);

		while (b.distributor->deserialized.find(tran1) ==
		       b.distributor->deserialized.end())
			b.distributor->cond.wait(lock);

		trancommit tr(b.repo->begin_commit(tran1,
						   x::eventfd::create()));

		std::map<std::string, std::string>::const_iterator
			p(tr->getOptions().find(tobjrepoObj::node_opt));

		if (p == tr->getOptions().end() || p->second !=
		    a.repocluster->getthisnodename()) // [TRANDISTSETNODE]
			throw EXCEPTION("[TRANDISTSETNODE] failed");
	}

	std::cerr << "Waiting for " << x::tostring(tran1)
		  << " to be cancelled" << std::endl;

	a.distributor->canceltransaction(tran1);

	{
		std::unique_lock<std::mutex> lock(b.distributor->mutex);

		while (b.distributor->cancelled.find(tran1) ==
		       b.distributor->cancelled.end())
			b.distributor->cond.wait(lock);
	}
}

void test2()
{
	x::uuid trana, tranb;

	{
		instance a(nodeadir);
		instance b(nodebdir);

		STASHER_NAMESPACE::nodeinfomap clusterinfo;

		STASHER_NAMESPACE::nodeinfo info;

		info.options.insert(std::make_pair(STASHER_NAMESPACE::nodeinfo::host_option,
						   "localhost"));

		clusterinfo[a.listener->nodeName()]=info;

		info.nomaster(true);

		clusterinfo[b.listener->nodeName()]=info;

		repoclusterinfoObj::saveclusterinfo(a.repo, clusterinfo);
		repoclusterinfoObj::saveclusterinfo(b.repo, clusterinfo);

		newtran na=a.repo->newtransaction(trana);
		newtran nb=b.repo->newtransaction(tranb);

		na->getOptions()[tobjrepoObj::node_opt]=
			a.listener->nodeName();
		nb->getOptions()[tobjrepoObj::node_opt]=
			a.listener->nodeName();

		na->finalize();
		nb->finalize();
	}

	instance a(nodeadir);
	instance b(nodebdir);

	a.start();
	b.start();
	a.listener->connectpeers();

	{
		std::unique_lock<std::mutex> lock(b.distributor->mutex);

		while (b.distributor->deserialized.find(trana) ==
		       b.distributor->deserialized.end() ||
		       b.distributor->cancelled.find(tranb) ==
		       b.distributor->cancelled.end())
			b.distributor->cond.wait(lock);
	}
}

static std::map<std::string, std::set<x::uuid> > recvtranslist;
static std::mutex recvtransm;
static std::condition_variable recvtransc;

static void recvtrans(const std::string &peername, const tranrecvcanc &c)
{
	std::unique_lock<std::mutex> m(recvtransm);

	for (std::map<x::uuid, dist_received_status_t>::const_iterator
		     b=c.received->uuids.begin(),
		     e=c.received->uuids.end(); b != e; ++b)
	{
		std::cerr << "Received " << x::tostring(b->first) << " from "
			  << peername << std::endl;

		recvtranslist[peername].insert(b->first);
	}

	for (std::set<x::uuid>::const_iterator
		     b=c.cancelled->uuids.begin(),
		     e=c.cancelled->uuids.end(); b != e; ++b)
	{
		std::cerr << "Cancelled " << x::tostring(*b) << " from "
			  << peername << std::endl;

		recvtranslist[peername].erase(*b);
	}

	recvtransc.notify_all();
}

void test3()
{
	instance a(nodeadir);
	instance b(nodebdir);

	{
		STASHER_NAMESPACE::nodeinfomap clusterinfo;

		STASHER_NAMESPACE::nodeinfo info;

		info.options.insert(std::make_pair(STASHER_NAMESPACE::nodeinfo::host_option,
						   "localhost"));

		clusterinfo[a.listener->nodeName()]=info;

		info.nomaster(true);

		clusterinfo[b.listener->nodeName()]=info;

		clusterinfo["nodec.test"]=info;
		clusterinfo["noded.test"]=info;

		repoclusterinfoObj::saveclusterinfo(a.repo, clusterinfo);
		repoclusterinfoObj::saveclusterinfo(b.repo, clusterinfo);
	}

	debug_recv_trans=recvtrans;

	a.start();
	b.start();

	x::uuid trana, tranb;

	{
		newtran na=a.repo->newtransaction(trana);

		na->newobj("obj1");

		a.distributor->newtransaction(na, x::ptr<x::obj>());
	}

	{
		newtran nb=b.repo->newtransaction(tranb);

		nb->newobj("obj2");

		b.distributor->newtransaction(nb, x::ptr<x::obj>());
	}

	std::cerr << "trana: " << x::tostring(trana) << std::endl;
	std::cerr << "tranb: " << x::tostring(tranb) << std::endl;

	a.listener->connectpeers();

	{
		std::unique_lock<std::mutex> m(recvtransm);

		// [SLAVERECVINIT]

		while (1)
		{
			std::set<x::uuid> &uuids=recvtranslist[b.listener->
							       nodeName()];

			if (uuids.find(trana) != uuids.end())
				break;

			if (uuids.find(tranb) != uuids.end())
				break;

			recvtransc.wait(m);
		}
	}

	x::uuid tranc;

	std::cerr << "tranc: " << x::tostring(tranc) << std::endl;

	{
		newtran nc=a.repo->newtransaction(tranc);

		nc->newobj("obj3");

		a.distributor->newtransaction(nc, x::ptr<x::obj>());
	}


	{
		std::unique_lock<std::mutex> m(recvtransm);

		while (1)
		{
			std::set<x::uuid> &uuids=recvtranslist[b.listener->
							       nodeName()];

			// [SLAVERECVUPDATE]
			if (uuids.find(tranc) != uuids.end())
				break;

			recvtransc.wait(m);
		}
	}

	a.distributor->canceltransaction(tranc);

	{
		std::unique_lock<std::mutex> m(recvtransm);

		while (1)
		{
			std::set<x::uuid> &uuids=recvtranslist[b.listener->
							       nodeName()];

			// [SLAVERECVUPDATE]

			if (uuids.find(tranc) == uuids.end())
				break;

			recvtransc.wait(m);
		}
	}
	debug_recv_trans=NULL;
}

void test4()
{
	instance a(nodeadir);
	instance b(nodebdir);

	{
		STASHER_NAMESPACE::nodeinfomap clusterinfo;

		STASHER_NAMESPACE::nodeinfo info;

		info.options.insert(std::make_pair(STASHER_NAMESPACE::nodeinfo::host_option,
						   "localhost"));

		clusterinfo[a.listener->nodeName()]=info;

		info.nomaster(true);

		clusterinfo[b.listener->nodeName()]=info;

		repoclusterinfoObj::saveclusterinfo(a.repo, clusterinfo);
		repoclusterinfoObj::saveclusterinfo(b.repo, clusterinfo);
	}

	a.start();
	b.start();
	a.waitquorumstatus(false);
	b.waitquorumstatus(false);
	a.listener->connectpeers();
	a.waitquorumstatus(true);
	b.waitquorumstatus(true);

	std::string aname=a.repocluster->getthisnodename();
	std::string bname=b.repocluster->getthisnodename();

	x::uuid tran1, tran2;

	{
		newtran tr=b.repo->newtransaction(tran1);

		tr->getOptions()[tobjrepoObj::node_opt]=aname;
		tr->newobj("test4tran1");
		tr->finalize();
	}

	{
		newtran tr=a.repo->newtransaction(tran2);

		tr->getOptions()[tobjrepoObj::node_opt]=bname;
		tr->newobj("test4tran2");
		tr->finalize();
	}

	repopeerconnectionptr bconn=
		repocontrollermasterptr(a.repocluster->getCurrentController())
		->debugGetPeerConnection(bname);

	if (bconn.null())
		throw EXCEPTION("Cannot find connection object to " + bname);

	x::ptr<x::obj> mcguffin(x::ptr<x::obj>::create());

	bconn->commit_peer(x::ref<repopeerconnectionObj::commitreqObj>
			   ::create(tran1, aname,
				    STASHER_NAMESPACE::req_processed_stat),
			   mcguffin);

	{
		auto flag=x::destroyCallbackFlag::create();

		mcguffin->ondestroy([flag]{flag->destroyed();});

		mcguffin=x::ptr<x::obj>();
		flag->wait(); // [COMMITPEER]
	}

	{
		tobjrepoObj::values_t valuesMap;
		std::set<std::string> notfound;
		std::set<std::string> s;

		s.insert("test4tran1");

		b.repo->values(s, false, valuesMap, notfound);


		if (valuesMap.find("test4tran1") == valuesMap.end())
			throw EXCEPTION("[COMMITPEER] failed");
	}

	repopeerconnectionptr aconn=
		x::ptr<repocontrollerslaveObj>(b.repocluster
					       ->getCurrentController())
		->debugGetPeer();

	mcguffin=x::ptr<x::obj>::create();

	aconn->commit_peer(x::ref<repopeerconnectionObj::commitreqObj>
			   ::create(tran2, bname,
				    STASHER_NAMESPACE::req_processed_stat),
			   mcguffin);

	{
		auto flag=x::destroyCallbackFlag::create();

		mcguffin->ondestroy([flag]{flag->destroyed();});

		mcguffin=x::ptr<x::obj>();
		flag->wait(); // [COMMITPEERRACE]
	}

	{
		tobjrepoObj::values_t valuesMap;
		std::set<std::string> notfound;
		std::set<std::string> s;

		s.insert("test4tran2");

		a.repo->values(s, false, valuesMap, notfound);

		if (valuesMap.find("test4tran2") != valuesMap.end())
			throw EXCEPTION("[COMMITPEERRACE] failed");
	}
}

void test5()
{
	instance a(nodeadir);
	instance b(nodebdir);

	{
		STASHER_NAMESPACE::nodeinfomap clusterinfo;

		STASHER_NAMESPACE::nodeinfo info;

		info.options.insert(std::make_pair(STASHER_NAMESPACE::nodeinfo::host_option,
						   "localhost"));

		clusterinfo[a.listener->nodeName()]=info;
		clusterinfo[b.listener->nodeName()]=info;

		repoclusterinfoObj::saveclusterinfo(a.repo, clusterinfo);
		repoclusterinfoObj::saveclusterinfo(b.repo, clusterinfo);
	}

	a.start();
	b.start();
	a.waitquorumstatus(false);
	b.waitquorumstatus(false);
	a.listener->connectpeers();
	a.waitquorumstatus(true);
	b.waitquorumstatus(true);

	debug_ping_cnt=0;

	x::ptr<x::obj> mcguffin1(x::ptr<x::obj>::create());
	x::ptr<x::obj> mcguffin2(x::ptr<x::obj>::create());

	repopeerconnectionptr p(a.repocluster->
				getnodepeer(b.listener->nodeName()));

	p->ping(mcguffin1);
	p->ping(mcguffin2);

	{
		auto flag=x::destroyCallbackFlag::create();

		mcguffin1->ondestroy([flag]{flag->destroyed();});

		mcguffin1=x::ptr<x::obj>();
		flag->wait(); // [PINGPONG]
	}

	{
		auto flag=x::destroyCallbackFlag::create();

		mcguffin2->ondestroy([flag]{flag->destroyed();});

		mcguffin2=x::ptr<x::obj>();
		flag->wait(); // [PINGPONG]
	}

	if (debug_ping_cnt != 2)
		throw EXCEPTION("[PINGPONG] failed");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		x::dir::base::rmrf(clusterdir);
		x::dir::base::rmrf(nodeadir);
		x::dir::base::rmrf(nodebdir);

		{
			time_t now(time(NULL));

			repomg::clustkey_generate(clusterdir, clustername,
						  now,
						  now + 365 * 24 * 60 * 60,
						  "rsa",
						  "medium",
						  "sha1");

			repomg::nodekey_generate(nodeadir, clusterdir, "",
						 nodea,
						 now, now + 7 * 24 * 60 * 60,
						 "medium",
						 "sha1");

			repomg::nodekey_generate(nodebdir, clusterdir, "",
						 nodeb,
						 now, now + 7 * 24 * 60 * 60,
						 "medium",
						 "sha1");
		}
		ALARM(120);

		std::cerr << "test1" << std::endl;
		test1();
		std::cerr << "test2" << std::endl;
		test2();
		std::cerr << "test3" << std::endl;
		test3();
		std::cerr << "test4" << std::endl;
		test4();
		std::cerr << "test5" << std::endl;
		test5();

		x::dir::base::rmrf(clusterdir);
		x::dir::base::rmrf(nodeadir);
		x::dir::base::rmrf(nodebdir);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

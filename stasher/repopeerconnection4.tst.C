/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"

#include <x/obj.H>
#include <x/ref.H>

#include "nodeinfoconn.H"
#include "nodeclusterstatus.H"
#include "clusternotifierfwd.H"
#include "clusterstatusnotifierfwd.H"
#include "clusterinfofwd.H"
#include "clusterlistener.H"
#include "batonfwd.H"

#define clusterinfo_H

class clusterinfoObj : virtual public x::obj {

public:
	std::string nodename;

	class status_update_lock {};

	const std::string &getthisnodename() const noexcept
	{
		return nodename;
	}

	typedef std::map<std::string, nodeinfoconn> cluster_t;

	class newnodeclusterstatus {

	public:
		//! Calculated node status
		nodeclusterstatus status;

		//! If this node is a slave, this is the master
		x::ptr<peerstatusObj> masterpeer;

		//! Constructor
		newnodeclusterstatus(const nodeclusterstatus &statusArg)
			: status(statusArg) {}

		//! Destructor
		~newnodeclusterstatus() noexcept {}
	};

	x::ptr<peerstatusObj> getnodepeer(const std::string &nodename)

	{
		return x::ptr<peerstatusObj>();
	}

	clusterinfoObj(const std::string &nodenameArg,
		       const STASHER_NAMESPACE::nodeinfomap &clusterinfo)
		: nodename(nodenameArg), flag(false) {}

	~clusterinfoObj() noexcept {}

	std::mutex mutex;
	std::condition_variable cond;
	bool flag;

	std::pair<bool, x::ptr<peerstatusObj> > curpeer;

	std::pair<bool, x::ptr<peerstatusObj> >
	installpeer(const std::string &nodename,
		    const x::ptr<peerstatusObj> &node);

	void wait_installpeer()
	{
		std::unique_lock<std::mutex> lock(mutex);

		while (!flag)
			cond.wait(lock);

		flag=false;
	}

	void peerstatusupdated(const x::ptr<peerstatusObj> &peerRef,
			       const nodeclusterstatus &peerStatus)

	{
	}

	void peernewmaster(const x::ptr<peerstatusObj> &peerRef,
			   const nodeclusterstatus &newmaster)

	{
	}

	void installnotifycluster(const clusternotifier &notifier)

	{
	}

	void installnotifyclusterstatus(const clusterstatusnotifier
					&notifier)

	{
	}

	bool installbaton(const batonptr &batonArg)
	{
		return false;
	}

	void pingallpeers(const x::ptr<x::obj> &)
	{
	}

	void installformermasterbaton(const batonptr &batonp)

	{
	}
};

#include "clusternotifier.C"
#include "clusterstatusnotifier.C"
#include "peerstatus.C"

#include "repopeerconnectionbase.C"
#define REPOPEER_REGRESSION_TEST
#include "repopeerconnection.C"
#undef REPOPEER_REGRESSION_TEST
#include "peerstatusannouncer.C"

#include "stoppablethreadtracker.C"

#include "repocontrollerbase.C"
#include "repocontrollerbasehandoff.C"
#include "repocontrollermaster.C"
#include "repocontrollerslave.C"
#include "mastersyncinfo.C"
#include "repoclusterquorumcallbackbase.C"

#include "trandistributor.C"
#include <x/serialize.H>
#include <x/options.H>

std::pair<bool, peerstatus>
clusterinfoObj::installpeer(const std::string &nodename,
			    const peerstatus &node)

{
	std::pair<bool, x::ptr<peerstatusObj> > p(curpeer);

	curpeer=std::make_pair(true, peerstatus());

	std::unique_lock<std::mutex> lock(mutex);

	flag=true;
	cond.notify_all();

	return p;
}

class flusher : public x::threadmsgdispatcherObj {

public:

	flusher()=default;
	~flusher() noexcept=default;

	void run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
		 const x::fd &fd)
	{
		msgqueue_auto msgqueue(this);
		threadmsgdispatcher_mcguffin=nullptr;

		struct pollfd pfd[2];

		pfd[0].fd=fd->getFd();
		pfd[0].events=POLLIN;
		pfd[1].fd=msgqueue->getEventfd()->getFd();
		pfd[1].events=POLLIN;

		while (1)
		{

			char buf[8192];

			size_t n=fd->read(buf, sizeof(buf));

			if (n == 0 && errno == 0)
				break;

			if (n)
				continue;

			if (errno == 0)
				break;

			if (msgqueue->empty())
			{
				poll(pfd, 2, -1);
				continue;
			}
			msgqueue.event();
		}
	}
};

class instance {

public:
	std::pair<x::fd, x::fd> socks;
	STASHER_NAMESPACE::stoppableThreadTrackerImpl tracker;
	x::ref<flusher> flushthr;
	clusterinfo cluster;

	std::pair<repopeerconnectionptr,
		  x::runthreadbase> makeconn(time_t timestamp,
					     const std::string &nodename)
	{
		auto conn=repopeerconnectionptr
			::create(nodename,
				 spacemonitor::create(x::df::create(".")));

		conn->timestamp=timestamp;

		auto runthread=
			tracker->start_thread(repopeerconnection(conn),
					      x::fdbase(socks.second),
					      x::fd::base::inputiter(socks.second),
					      tracker->getTracker(),
					      x::ptr<x::obj>::create(),
					      false,
					      x::ptr<trandistributorObj>(),
					      clusterlistenerptr(),
					      nodeclusterstatus(nodename,
								x::uuid(),
								0,
								false),
					      cluster,
					      tobjrepo::create("repo.tst"));

		cluster->wait_installpeer();

		return std::make_pair(conn, runthread);
	}

	instance(const std::string &nodename)
		: socks(x::fd::base::socketpair()),
		  tracker(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create()),
		  flushthr(x::ref<flusher>::create()),
		  cluster(clusterinfo::create(nodename,
					      STASHER_NAMESPACE::nodeinfomap()))
	{
		socks.first->nonblock(true);
		socks.second->nonblock(true);

		tracker->start_thread(flushthr, socks.second);

		cluster->curpeer.first=false;
	}

	void clearconn()
	{
		cluster->curpeer.first=true;
		cluster->curpeer.second=repopeerconnectionptr();
	}

	void setconn(const repopeerconnection &conn)
	{
		cluster->curpeer.first=false;
		cluster->curpeer.second=conn;
	}
};

static void test1()
{
	instance one("node1");

	auto conn=one.makeconn(0, "node2");
	conn.second->wait();
}

static void test2()
{
	instance one("node1");

	one.clearconn();

	auto conn1=one.makeconn(1, "node2");

	one.setconn(conn1.first);

	auto conn0=one.makeconn(0, "node2");

	conn0.second->wait(); // [CONNCONFLICT]
}

static void test3()
{
	instance one("node1");

	one.clearconn();

	auto conn10=one.makeconn(10, "node2");

	one.setconn(conn10.first);

	auto conn12=one.makeconn(12, "node2");

	conn10.second->wait(); // 10 superceded by 12
	one.clearconn();
	conn10.first=repopeerconnectionptr();
	one.cluster->wait_installpeer(); // 12 should now install itself
	// [CONNCONFLICT]

	one.setconn(conn12.first);

	auto conn11=one.makeconn(11, "node2");
	conn11.second->wait(); // 11 superceded by 12
}

static void test4()
{
	instance one("node1");

	one.clearconn();

	auto first=one.makeconn(0, "node2");

	one.setconn(first.first);

	auto second=one.makeconn(0, "node2");

	// Connection with the same timestamp from a higher node wins
	first.second->wait();
	// [CONNCONFLICTTIE]
}

static void test5()
{
	instance two("node2");

	two.clearconn();

	auto first=two.makeconn(0, "node1");

	two.setconn(first.first);

	auto second=two.makeconn(0, "node1");

	auto destroycb=x::destroyCallbackFlag::create();

	first.first->ondestroy([destroycb]{destroycb->destroyed();});
	second.first->ondestroy([destroycb]{destroycb->destroyed();});

	first.first=second.first=repopeerconnectionptr();

	// one of them should be destroyed

	destroycb->wait();
	// [CONNCONFLICTTIE]
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		x::dir::base::rmrf("repo.tst");
		ALARM(120);

		std::cout << "test1" << std::endl;
		test1();

		std::cout << "test2" << std::endl;
		test2();

		std::cout << "test3" << std::endl;
		test3();

		std::cout << "test4" << std::endl;
		test4();

		std::cout << "test5" << std::endl;
		test5();
		x::dir::base::rmrf("repo.tst");
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

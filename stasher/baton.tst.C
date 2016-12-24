/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include <x/options.H>
#include <x/destroycallbackflagobj.H>

static std::mutex debug_ping_mutex;
static int debug_ping_cnt=0;

#define DEBUG_PINGPONG() do {					\
		std::unique_lock<std::mutex> lock(debug_ping_mutex);	\
								\
		++debug_ping_cnt;				\
	} while (0)

static std::mutex baton_test1_mutex;
static std::condition_variable baton_test1_cond;

static bool baton_test1_thread_trap=false;
static bool baton_test1_thread_trapped=false;
static bool baton_test1_blocked_trap=false;
static bool baton_test1_enabled=false;

/*
** After commits are blocked, signal the main thread to submit a transaction,
** which should get trapped by DEBUG_BATON_TEST_1_HOOK_COMMIT_BLOCKED, and this
** thread waits until that happens.
*/

#define DEBUG_BATON_TEST_1_HOOK_LOCK_THREAD() do {			\
		std::unique_lock<std::mutex> lock(baton_test1_mutex);		\
									\
		if (baton_test1_thread_trap)				\
		{							\
			baton_test1_thread_trap=false;			\
			baton_test1_thread_trapped=true;		\
			std::cerr << "commit thread: lock acquired"	\
				  << std::endl;				\
			baton_test1_cond.notify_all();			\
			while (baton_test1_thread_trapped)		\
				baton_test1_cond.wait(lock);		\
			std::cerr << "commit thread: continuing"	\
				  << std::endl;				\
		}							\
	} while (0)

/* When the baton is being handed over, commits are blocked */

#define DEBUG_BATON_TEST_1_HOOK_COMMIT_BLOCKED() do {			\
		std::unique_lock<std::mutex> lock(baton_test1_mutex);		\
									\
		if (baton_test1_blocked_trap)				\
		{							\
			/* [MASTERHANDOFFBLOCK] */			\
			baton_test1_blocked_trap=false;			\
			baton_test1_thread_trapped=false;		\
			std::cerr << "commit held" << std::endl;	\
			baton_test1_cond.notify_all();			\
		}							\
	} while (0)

/* Abort handoff after commits are blocked */

#define DEBUG_BATON_TEST_1_COMPLETED() do {			\
		std::unique_lock<std::mutex> lock(baton_test1_mutex);	\
								\
		if (baton_test1_enabled)			\
		{						\
			baton_test1_enabled=false;		\
			dispatch_handoff_failed();		\
			std::cerr << "baton handoff backed out"	\
				  << std::endl << std::flush;	\
			baton_test1_cond.notify_all();		\
			return;					\
		}						\
	} while (0)


static std::mutex test2_mutex;
static std::condition_variable test2_cond;

static bool test2_enabled=false;
static bool test2_dispatch_handoff_failed_called;
static bool test2_baton_announced;
static bool test2_peer_announced;
static std::string test2_peer_name;
static bool test2_peer_releaseed;
static bool test2_commitlock_received;
static bool test2_release_received;

/* Verify that the master received a handover failure notice */

#define DEBUG_BATON_TEST_2_FAILED_HOOK() do {				\
		std::unique_lock<std::mutex> lock(test2_mutex);			\
									\
		if (test2_enabled)					\
		{							\
			test2_dispatch_handoff_failed_called=true;	\
			test2_cond.notify_all();				\
			std::cout << "Restoring commits"		\
				  << std::endl;				\
		}							\
	} while (0)

/*
** After the baton has been announced, simulate an abort, a master failuer.
*/

#define DEBUG_BATON_TEST_2_ANNOUNCED_HOOK() do {			\
		std::unique_lock<std::mutex> lock(test2_mutex);			\
									\
		if (test2_enabled)					\
		{							\
			/* [MASTERHANDOFFANNOUNCE */			\
			std::cout << "Announce messages processed"	\
				  << std::endl;				\
			test2_baton_announced=true;			\
			test2_cond.notify_all();				\
			return;						\
			/* Exit path, [MASTERHANDOFFABORT] */		\
		}							\
	} while (0)

/* Capture the baton announcement message, and to which peer it goes */

#define DEBUG_BATON_TEST_2_ANNOUNCE_HOOK() do {				\
		std::unique_lock<std::mutex> lock(test2_mutex);			\
									\
		if (test2_enabled)					\
		{							\
			/* [MASTERANNOUNCEBATON] */			\
			std::cout << "Announce message received"	\
				  << std::endl;				\
			test2_peer_announced=true;			\
			test2_peer_name=peername;			\
			test2_cond.notify_all();				\
		}							\
	} while (0)

/*
** Connection to third party slave received a baton release message after
** handover was aborted.
*/

#define DEBUG_BATON_TEST_2_RELEASE_HOOK() do {				\
		std::unique_lock<std::mutex> lock(test2_mutex);			\
									\
		if (test2_enabled)					\
		{							\
			/* [BATONRELEASESEND */				\
			std::cout << "Release message sent"		\
				  << std::endl;				\
			test2_peer_releaseed=true;			\
			test2_cond.notify_all();				\
		}							\
	} while (0)

/*
** The third party slave acquired a commit lock after getting a baton
** announcement.
*/

#define DEBUG_BATON_TEST_2_COMMITLOCK_HOOK() do {			\
		std::unique_lock<std::mutex> lock(test2_mutex);			\
									\
		if (test2_enabled)					\
		{							\
			/* [BATONSLAVEACQUIRECOMMIT] */			\
			std::cout << "Commit lock acquired"		\
				  << std::endl;				\
			test2_commitlock_received=true;			\
			test2_cond.notify_all();				\
		}							\
	} while (0)

/*
** Slave received the baton release notice.
*/

#define DEBUG_BATON_TEST_2_RELEASE_RECEIVED_HOOK() do {			\
		std::unique_lock<std::mutex> lock(test2_mutex);			\
									\
		if (test2_enabled)					\
		{							\
			/* [BATONRELEASERECV] */				\
			std::cout << "Release message received"		\
				  << std::endl;				\
			test2_release_received=true;			\
			test2_cond.notify_all();				\
		}							\
	} while (0)

static std::mutex test3_mutex;
static std::condition_variable test3_cond;

static bool test3_enabled=false;
static x::ptr<x::obj> *test3_keepbaton;
static bool test3_baton_announced;
static bool test3_release_received;
static bool test3_cancelled;

#define DEBUG_BATON_TEST_3_ANNOUNCED_HOOK() do {			\
		std::unique_lock<std::mutex> lock(test3_mutex);			\
									\
		if (test3_enabled)					\
		{							\
			/* [MASTERHANDOFFANNOUNCE */			\
			if (test3_keepbaton)				\
				*test3_keepbaton=batonp;		\
			std::cout << "Announce messages processed"	\
				  << std::endl;				\
			test3_baton_announced=true;			\
			test3_cond.notify_all();			\
			return;						\
		}							\
	} while (0)

#define DEBUG_BATON_TEST_3_COMMITLOCK_HOOK() do {			\
		/* [BATONSLAVEACQUIRECOMMIT] */				\
		std::cerr << "Baton installed on "			\
			  << getthisnodename()				\
			  << ": from "					\
			  << batonp->resigningnode			\
			  << " to "					\
			  << batonp->replacementnode << std::endl;	\
	} while (0)

#define DEBUG_BATON_TEST_3_RELEASE_RECEIVED_HOOK() do {			\
		std::unique_lock<std::mutex> lock(test3_mutex);			\
									\
		if (test3_enabled)					\
		{							\
			/* [BATONRELEASEFWD] */				\
			std::cout << "Release message received"		\
				  << std::endl;				\
			test3_release_received=true;			\
			test3_cond.notify_all();			\
		}							\
	} while (0)

class test3_dcb : virtual public x::obj {

public:
	test3_dcb() {}
	~test3_dcb() noexcept {}

	void destroyed()
	{
		std::unique_lock<std::mutex> lock(test3_mutex);

		std::cout << "Baton cancelled" << std::endl;
		test3_cancelled=true;
		test3_cond.notify_all();
	}
};

#define DEBUG_BATON_TEST_3_CANCELLED_HOOK() do {			\
		if (test3_enabled)					\
		{							\
			auto cb=x::ref<test3_dcb>::create();		\
			batonp->ondestroy([cb]{cb->destroyed();});	\
		}							\
	} while (0)

static std::mutex test4_mutex;
static std::condition_variable test4_cond;

static bool test4_enabled=false;
std::map<std::string, std::map<std::string,
			       nodeclusterstatus> > test4_statusmap;
std::map<std::string, std::map<std::string,
			       nodeclusterstatus> > test4_statusmap_recv;
static bool test4_baton_given;

#define DEBUG_BATON_TEST_4_GIVEN_CB() do {			\
		std::unique_lock<std::mutex> lock(test4_mutex);		\
								\
		if (test4_enabled)				\
		{						\
			test4_statusmap_recv=test4_statusmap;	\
			test4_baton_given=true;			\
			test4_cond.notify_all();			\
			return;					\
		}						\
	} while (0)

#define DEBUG_BATON_TEST_4_CLUSTERSTATUS_HOOK() do {		\
		std::unique_lock<std::mutex> lock(test4_mutex);		\
								\
		if (test4_enabled)				\
		{						\
			test4_statusmap[getthisnodename()]	\
				[peername]=newStatus;		\
		}						\
	} while (0)

static std::mutex test5_mutex;
static std::condition_variable test5_cond;

static bool test5_enabled=false;

static bool test5_failed_hook;

/* Simulate a failure in handing over the baton to the new master */

#define DEBUG_BATON_TEST_5_ABORT_BATON_TRANSFER_HOOK() do {		\
		std::unique_lock<std::mutex> lock(test5_mutex);			\
									\
		if (test5_enabled)					\
		{							\
			std::cerr << "Simulating a handover failure"	\
				  << std::endl;				\
			return;						\
		}							\
	} while (0)

/* Handover failed, we should be back to normal */

#define DEBUG_BATON_TEST_5_FAILED_HOOK() do { \
		std::unique_lock<std::mutex> lock(test5_mutex);		\
								\
		if (test5_enabled)				\
		{						\
			test5_failed_hook=true;			\
			test5_cond.notify_all();			\
		}						\
	} while (0)

static std::mutex test6_mutex;
static std::condition_variable test6_cond;

static bool test6_enabled=false;

static bool test6_slave_disconnect;
static bool test6_oldmaster_disconnect;
static bool test6_newmaster_disconnect;
static int test6_baton_transfer_counter;

#define DEBUG_BATON_TEST_6_SLAVE_FORMERMASTER_DISCONNECT() do {	\
		std::unique_lock<std::mutex> lock(test6_mutex);		\
								\
		if (test6_enabled)				\
		{						\
			test6_slave_disconnect=true;		\
			std::cerr << "Slave dropped the baton"	\
				  << std::endl;			\
			test6_cond.notify_all();			\
		}						\
	} while (0)

#define DEBUG_BATON_TEST_6_NEWMASTER_FORMERMASTER_DISCONNECT() do {	\
		std::unique_lock<std::mutex> lock(test6_mutex);			\
									\
		if (test6_enabled)					\
		{							\
			test6_oldmaster_disconnect=true;		\
			std::cerr << "Former master dropped the baton"	\
				  << std::endl;				\
			test6_cond.notify_all();				\
		}							\
	} while (0)


#define DEBUG_BATON_TEST_6_NEWMASTER_DISCONNECT() do {			\
		std::unique_lock<std::mutex> lock(test6_mutex);			\
									\
		if (test6_enabled)					\
		{							\
			test6_newmaster_disconnect=true;		\
			std::cerr << "New master dropped the baton"	\
				  << std::endl;				\
			test6_cond.notify_all();				\
		}							\
	} while (0)

#define DEBUG_BATON_TEST_6_VERIFY() do { \
		std::unique_lock<std::mutex> lock(test6_mutex);			\
									\
		if (test6_enabled)					\
		{							\
			++test6_baton_transfer_counter;			\
			test6_cond.notify_all();				\
		}							\
	} while (0)

#include "repocontrollermaster.C"
#include "repopeerconnection.C"
#include "baton.C"
#include "objrepocopysrcthread.C"

class nodechangetrack : virtual public x::obj {

public:
	std::string record;
	std::mutex mutex;

	nodechangetrack() {}
	~nodechangetrack() noexcept {}
};

class keeptrackofmasterObj : public clusterstatusnotifierObj {

public:
	std::string whichnode;
	std::string master;

	x::ptr<nodechangetrack> track;

	keeptrackofmasterObj(const std::string &whichnodeArg,
			     const x::ptr<nodechangetrack> &trackArg)

		: whichnode(whichnodeArg), track(trackArg) {}

	~keeptrackofmasterObj() noexcept {}

        void initialstatus(const nodeclusterstatus &newStatus)

	{
		master=newStatus.master;
	}

        void statusupdated(const nodeclusterstatus &newStatus)

	{
		if (newStatus.master == master)
			return;

		master=newStatus.master;

		std::unique_lock<std::mutex> lock(track->mutex);

		track->record += whichnode + " " + master + "\n";
	}
};

void test1()
{
	node a(tstnodes::getnodedir(0)), b(tstnodes::getnodedir(1));

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

	a.start(true);
	b.start(true);
	a.debugWaitFullQuorumStatus(false);
	b.debugWaitFullQuorumStatus(false);
	a.listener->connectpeers();

	std::cerr << "Waiting for quorum" << std::endl;
	a.debugWaitFullQuorumStatus(true);
	b.debugWaitFullQuorumStatus(true);

	repocontrollermasterptr
		master(a.repocluster->getCurrentController());

	{
		std::unique_lock<std::mutex> lock(baton_test1_mutex);

		baton_test1_thread_trap=true;
		baton_test1_enabled=true;

		master->handoff_request(b.listener->nodeName());

		while (!baton_test1_thread_trapped)
			baton_test1_cond.wait(lock);
		baton_test1_blocked_trap=true;
		std::cerr << "creating transaction" << std::endl;
	}

	trandistributorObj::transtatus stat;
	x::ptr<x::obj> mcguffin;

	mcguffin=x::ptr<x::obj>::create();
	{
		newtran tr(a.repo->newtransaction());

		(*tr->newobj("test1")->getostream())
			<< "baz" << std::flush;

		stat=a.distributor->newtransaction(tr, mcguffin);
	}

	std::cerr << "Transaction created" << std::endl;

	{
		std::unique_lock<std::mutex> lock(baton_test1_mutex);

		while (baton_test1_enabled)
			baton_test1_cond.wait(lock);
	}

	x::destroyCallbackFlag flag(x::destroyCallbackFlag::create());

	mcguffin->ondestroy([flag]{flag->destroyed();});

	mcguffin=nullptr;
	flag->wait();

	if (stat->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION("Transaction commit failed");
}

static void put_object(node &a, const std::string &objname,
		       const std::string &objvalue)
{
	trandistributorObj::transtatus stat;
	x::ptr<x::obj> mcguffin;

	mcguffin=x::ptr<x::obj>::create();
	{
		newtran tr(a.repo->newtransaction());

		(*tr->newobj(objname)->getostream())
			<< objvalue << std::flush;

		stat=a.distributor->newtransaction(tr, mcguffin);
	}

	x::destroyCallbackFlag flag(x::destroyCallbackFlag::create());

	mcguffin->ondestroy([flag]{flag->destroyed();});

	mcguffin=nullptr;
	flag->wait();

	if (stat->status != STASHER_NAMESPACE::req_processed_stat)
		// [MASTERHANDOFFABORT]
		throw EXCEPTION("Transaction commit failed");
}

static void check_repo(const tobjrepo &repo,
		       const std::string &reponame,
		       const std::string &objname,
		       const std::string &objval)
{
	std::set<std::string> s;
	tobjrepoObj::values_t valuesMap;
	std::set<std::string> notfound;

	s.insert(objname);

	repo->values(s, true, valuesMap, notfound);

	tobjrepoObj::values_t::iterator iter=valuesMap.find(objname);

	if (iter == valuesMap.end())
		throw EXCEPTION("Error - object " + objname
				+ " not found in node " + reponame);

	std::string str;

	std::getline(*iter->second.second->getistream(), str);

	if (str != objval)
		throw EXCEPTION("Error - object " + objname
				+ " in node " + reponame
				+ " contains " + str
				+ " instead of the expected value: " + objval);
}

void test2(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	node &a=*tnodes[0], &b=*tnodes[1], &c=*tnodes[2];

	repocontrollermasterptr
		master=t.startmastercontrolleron0(tnodes);


	{
		std::unique_lock<std::mutex> lock(test2_mutex);

		test2_enabled=true;

		test2_dispatch_handoff_failed_called=false;
		test2_baton_announced=false;
		test2_peer_announced=false;
		test2_peer_releaseed=false;
		test2_commitlock_received=false;
		test2_release_received=false;

		master->handoff_request(b.listener->nodeName());
		master=repocontrollermasterptr();

		std::cerr << "Started handoff" << std::endl;

		while (!test2_dispatch_handoff_failed_called ||
		       !test2_baton_announced ||
		       !test2_peer_announced ||
		       !test2_peer_releaseed ||
		       !test2_commitlock_received ||
		       !test2_release_received)
		{
			test2_cond.wait(lock);
		}
		std::cout << "Handoff aborted" << std::endl;

		if (test2_peer_name != c.listener->nodeName())
			throw EXCEPTION("[MASTERHANDOFANNOUNCE] failed");
		test2_enabled=false;
	}

	put_object(a, "test2", "baz");

	check_repo(a.repo, "a", "test2", "baz");
	check_repo(b.repo, "b", "test2", "baz");
	check_repo(c.repo, "c", "test2", "baz");
}

void test3p1(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	node &b=*tnodes[1];

	repocontrollermasterptr
		master=t.startmastercontrolleron0(tnodes);

	{
		std::unique_lock<std::mutex> lock(test3_mutex);

		test3_enabled=true;
		test3_keepbaton=NULL;
		test3_baton_announced=false;
		test3_release_received=false;
		test3_cancelled=false;

		master->handoff_request(b.listener->nodeName());
		master=repocontrollermasterptr();

		std::cerr << "Started handoff" << std::endl;

		while (!test3_baton_announced ||
		       !test3_release_received ||
		       !test3_cancelled)
		{
			test3_cond.wait(lock);
		}

		test3_enabled=false;
	}
}

void test3p2(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	node &a=*tnodes[0], &b=*tnodes[1], &c=*tnodes[2];

	repocontrollermasterptr
		master=t.startmastercontrolleron0(tnodes);

	{
		x::ptr<x::obj> mcguffin(x::ptr<x::obj>::create());

		{
			std::unique_lock<std::mutex> lock(debug_ping_mutex);

			debug_ping_cnt=0;
		}

		c.repocluster->pingallpeers(mcguffin);

		{
			auto flag=x::destroyCallbackFlag::create();

			mcguffin->ondestroy([flag]{flag->destroyed();});

			mcguffin=nullptr;
			flag->wait(); // [PINGALLPEERS]
		}

		{
			std::unique_lock<std::mutex> lock(debug_ping_mutex);

			if (debug_ping_cnt != 2)
				throw EXCEPTION("[PINGALLPEERS] failed");
		}
	}

	{
		std::unique_lock<std::mutex> lock(test3_mutex);
		x::ptr<x::obj> capture_baton;

		test3_enabled=true;
		test3_keepbaton=&capture_baton;
		test3_baton_announced=false;
		test3_release_received=false;
		test3_cancelled=false;

		master->handoff_request(b.listener->nodeName());
		master=repocontrollermasterptr();

		std::cerr << "Started handoff" << std::endl;

		while (capture_baton.null())
			test3_cond.wait(lock);

		std::cerr << "Breaking the connection" << std::endl;

		x::stoppable(c.repocluster->getnodepeer(a.listener->nodeName()))
			->stop();

		while (!test3_cancelled)
			test3_cond.wait(lock); // [BATONCANCELOLDMASTERGONE]

		test3_enabled=false;
	}
}

static void test4(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	node &a=*tnodes[0], &b=*tnodes[1], &c=*tnodes[2];

	repocontrollermasterptr
		master=t.startmastercontrolleron0(tnodes);

	t.any1canbemaster(tnodes);

	std::cout << "Handing over the baton" << std::endl;

	std::unique_lock<std::mutex> lock(test4_mutex);

	test4_enabled=true;
	test4_statusmap.clear();
	test4_baton_given=false;

	master->handoff_request(b.listener->nodeName());
	master=repocontrollermasterptr();

	while (!test4_baton_given) // [BATONNEWMASTERREADY]
		test4_cond.wait(lock);

	if (test4_statusmap_recv[a.listener->nodeName()]
	    [b.listener->nodeName()].master != b.listener->nodeName())
		throw EXCEPTION("[BATONNEWNOTIFIED] failed (1)");

	if (test4_statusmap_recv[c.listener->nodeName()]
	    [b.listener->nodeName()].master != b.listener->nodeName())
		throw EXCEPTION("[BATONNEWNOTIFIED] failed (2)");

	test4_enabled=false;
}

static void test5(tstnodes &t, int n)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	node &a=*tnodes[0], &b=*tnodes[1], &c=*tnodes[2];

	repocontrollermasterptr
		master=t.startmastercontrolleron0(tnodes);

	t.any1canbemaster(tnodes);

	std::cout << "Handing over the baton" << std::endl;

	{
		std::unique_lock<std::mutex> lock(test5_mutex);

		test5_enabled=true;
		test5_failed_hook=false;

		master->handoff_request(b.listener->nodeName());
		master=repocontrollermasterptr();

		while (!test5_failed_hook)
			test5_cond.wait(lock);

		test5_enabled=false;
	}

	std::cerr << "Waiting until quorum is reestablished"
		  << std::endl;

	a.debugWaitFullQuorumStatus(true);
	b.debugWaitFullQuorumStatus(true);
	c.debugWaitFullQuorumStatus(true);

	std::cerr << "Commiting a sample transaction" << std::endl;

	std::string objname=({
			std::ostringstream o;

			o << "test5" << n;

			o.str(); });

	put_object(a, objname, "baz");
	check_repo(a.repo, "a", objname, "baz");
	check_repo(b.repo, "b", objname, "baz");
	check_repo(c.repo, "c", objname, "baz");

}

static void test6(tstnodes &t, int n)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	node &a=*tnodes[0], &b=*tnodes[1], &c=*tnodes[2];

	repocontrollermasterptr
		master=t.startmastercontrolleron0(tnodes);

	t.any1canbemaster(tnodes);

	x::ptr<nodechangetrack> track=x::ptr<nodechangetrack>::create();

	auto atrack=x::ref<keeptrackofmasterObj>::create("a", track),
		btrack=x::ref<keeptrackofmasterObj>::create("b", track),
		ctrack=x::ref<keeptrackofmasterObj>::create("c", track);

	a.repocluster->installnotifyclusterstatus(atrack);
	b.repocluster->installnotifyclusterstatus(btrack);
	c.repocluster->installnotifyclusterstatus(ctrack);

	std::cout << "Handing over the baton" << std::endl;

	{
		std::unique_lock<std::mutex> lock(test6_mutex);

		test6_enabled=true;
		test6_slave_disconnect=false;
		test6_oldmaster_disconnect=false;
		test6_newmaster_disconnect=false;
		test6_baton_transfer_counter=0;

		master->handoff_request(b.listener->nodeName());
		master=repocontrollermasterptr();

		while (!test6_slave_disconnect || // [BATONSLAVECUTOVER]
		       !test6_oldmaster_disconnect || // [BATONNEWMASTERCUTOVER]
		       !test6_newmaster_disconnect || // [BATONOLDMASTERCUTOVER]
		       test6_baton_transfer_counter < 2)
			test6_cond.wait(lock);

		test6_enabled=false;
	}

	std::cerr << "Waiting until quorum is reestablished"
		  << std::endl;

	a.debugWaitFullQuorumStatus(true);
	b.debugWaitFullQuorumStatus(true);
	c.debugWaitFullQuorumStatus(true);

	std::cerr << "Commiting a sample transaction" << std::endl;

	std::string objname=({
			std::ostringstream o;

			o << "test6" << n;

			o.str(); });

	put_object(a, objname, "baz");
	check_repo(a.repo, "a", objname, "baz");
	check_repo(b.repo, "b", objname, "baz");
	check_repo(c.repo, "c", objname, "baz");

	{
		std::unique_lock<std::mutex> lock(track->mutex);

		if (track->record !=
		    "b " + b.repocluster->getthisnodename() + "\n"
		    "a " + b.repocluster->getthisnodename() + "\n"
		    "c " + b.repocluster->getthisnodename() + "\n")
			throw "test6 failed";
	}
}

static void test7(tstnodes &t, int n)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	t.startmastercontrolleron0(tnodes);
	t.any1canbemaster(tnodes);

	boolref status=boolref::create();

	std::string newmaster=tstnodes::getnodefullname(1);

	auto cb=x::destroyCallbackFlag::create();

	tnodes[n]->repocluster->
		master_handover_request(newmaster, status)
		->ondestroy([cb]{cb->destroyed();});

	cb->wait(); // [BATONHANDOVERTHREAD]

	if (!status->flag)
		throw EXCEPTION("[BATONHANDOVERTHREAD] did not succeed");

	for (size_t i=0; i<tnodes.size(); i++)
	{
		std::string nodemaster=
			repoclusterinfoObj::status(tnodes[i]->repocluster)
			->master;

		if (nodemaster != newmaster)
			throw EXCEPTION("[BATONHANDOVERTHREAD] failed: node master is "
					+ nodemaster + " instead of "
					+ newmaster);
	}
}

static void test8(tstnodes &t, size_t n)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	t.startmastercontrolleron0(tnodes);
	t.any1canbemaster(tnodes);

	boolref status=boolref::create();

	std::string newmaster="xxxx";

	auto cb=x::destroyCallbackFlag::create();

	tnodes[0]->repocluster->
		master_handover_request(newmaster, status)
		->ondestroy([cb]{cb->destroyed();});

	cb->wait();

	if (status->flag)
		throw EXCEPTION("test8 did not fail, as expected");

	std::ostringstream o;

	o << "test8." << n;

	put_object(*tnodes[0], o.str(), "dummy");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		tstnodes nodes(3);

		// std::cout << "test1" << std::endl;
		// test1();

		std::cout << "test2" << std::endl;
		test2(nodes);

		std::cout << "test3, part 1" << std::endl;
		test3p1(nodes);

		std::cout << "test3, part 2" << std::endl;
		test3p2(nodes);

		std::cout << "test4" << std::endl;
		test4(nodes);

		std::cout << "test5" << std::endl;
		test5(nodes, 0);

		std::cout << "test6" << std::endl;
		test6(nodes, 0);

		std::cout << "test7, part 1" << std::endl;
		test7(nodes, 0);

		std::cout << "test7, part 2" << std::endl;
		test7(nodes, 1);

		std::cout << "test7, part 3" << std::endl;
		test7(nodes, 2);

		for (int i=0; i<3; ++i)
		{
			std::cout << "test8, " << i << std::endl;
			test8(nodes, i);
		}
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

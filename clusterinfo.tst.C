/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "peerstatus.H"
#include "objrepocopysrcinterface.H"
#include "clusternotifier.H"
#include "clusterstatusnotifier.H"
#include "stasher/nodeinfo.H"
#include "baton.H"

#include <x/options.H>

static bool recalculate_called_hook;

#define DEBUG_RECALCULATE_HOOK() recalculate_called_hook=true;

#include "clusterinfo.C"
#include <iostream>
#include <cstdlib>

class mypeerObj : public peerstatusObj::adapterObj, public x::stoppableObj {

public:
	bool stop_received;

	bool initial_status_received;

	bool status_update_received;

	mypeerObj(const std::string &peernameArg)

		: peerstatusObj::adapterObj(peernameArg),
		stop_received(false),
		initial_status_received(false),
		status_update_received(false) {}
	~mypeerObj() {}

	void stop()
	{
		stop_received=true;
	}

	using peerstatusObj::install;

	using peerstatusObj::peerstatusupdate;

	nodeclusterstatus initialstatus_value;
	nodeclusterstatus statusupdate_value;

	void statusupdated(const nodeclusterstatus &newStatus)

	{
		statusupdate_value=newStatus;
		status_update_received=true;
	}

	void initialstatus(const nodeclusterstatus &newStatus)

	{
		initialstatus_value=newStatus;
		initial_status_received=true;
	}

};

typedef x::ptr<mypeerObj> mypeer;

class mynotifierObj : public clusterstatusnotifierObj {

public:
	bool notified;
	nodeclusterstatus status;

	mynotifierObj() : notified(false)
	{
	}

	~mynotifierObj() {}

	void statusupdated(const nodeclusterstatus &newStatus)

	{
		status=newStatus;
		notified=true;
	}
};

typedef x::ptr<mynotifierObj> mynotifier;

class currentclusterObj : public clusternotifierObj {

public:
	int invoked;

	currentclusterObj() : invoked(0) {}
	~currentclusterObj() {}

	clusterinfoObj::cluster_t curstatus;

        void clusterupdated(const clusterinfoObj::cluster_t &newStatus)

	{
		++invoked;
		curstatus=newStatus;
	}
};

static bool equal_sans_timestamps(nodeclusterstatus a,
				  const nodeclusterstatus &b)
{
	a.timestamp=b.timestamp;

	return a == b;
}

static void test1()
{
	auto cc=x::ref<currentclusterObj>::create();

	STASHER_NAMESPACE::stoppableThreadTrackerImpl tracker=STASHER_NAMESPACE::stoppableThreadTrackerImpl::create();

	clusterinfo c(clusterinfo::create("a", "test", tracker->getTracker(),
					  STASHER_NAMESPACE::nodeinfomap()));

	nodeclusterstatus status1(*clusterinfoObj::status(c));

	if (!equal_sans_timestamps(status1, nodeclusterstatus("a", x::uuid(), 0, false)))
		throw EXCEPTION("Empty cluster's status is something weird");

	c->installnotifycluster(cc);

	if (cc->invoked != 1 || cc->curstatus.size() != 0)
		throw EXCEPTION("[UPDATECALLBACK]: initial callback failed");


	{
		mypeer p=mypeer::create("b");

		p->peerstatusupdate(nodeclusterstatus("b", x::uuid(),
						      1, false));

		if (p->install(c).first || // [FAILED]
		    *clusterinfoObj::status(c) != status1)
			throw EXCEPTION("[NONEXISTENT] test failed");

		if (clusterinfoObj::status(c)->uuid != status1.uuid)
			throw EXCEPTION("[NONEXISTENT] test failed");

		if (!c->getnodepeer("b").null())
			throw EXCEPTION("[GETNODEPEER] failed");

		nodeclusterstatus status2("b", x::uuid(), 1, false);

		p=mypeer::create("b");
		p->peerstatusupdate(status2);

		STASHER_NAMESPACE::nodeinfomap m;

		m["b"]=STASHER_NAMESPACE::nodeinfo();
		m["c"]=STASHER_NAMESPACE::nodeinfo();

		c->update(m); // [CLUSTERUPDATE]

		// [UPDATECALLBACK]
		if (cc->invoked != 2 || cc->curstatus.size() != 2 ||
		    cc->curstatus.find("b") == cc->curstatus.end() ||
		    cc->curstatus.find("c") == cc->curstatus.end())
			throw EXCEPTION("[UPDATECALLBACK]: callback failed");

		if (!p->install(c).first ||
		    *clusterinfoObj::status(c) != status2)
			throw EXCEPTION("[CLUSTERUPDATE] test failed");

		if (clusterinfoObj::status(c)->uuid != status2.uuid)
			throw EXCEPTION("[CLUSTERUPDATE] test failed");

		// [UPDATECALLBACK2]
		if (cc->invoked != 3 || cc->curstatus.size() != 2 ||
		    cc->curstatus.find("b") == cc->curstatus.end() ||
		    cc->curstatus.find("c") == cc->curstatus.end())
			throw EXCEPTION("[UPDATECALLBACK2] failed (1 of 3)");

		c->update(m);
		if (cc->invoked != 3)
			throw EXCEPTION("[UPDATECALLBACK]: spurious callback");


		mynotifier n(mynotifier::create());

		c->installnotifyclusterstatus(n); // [REGISTER], [FIRSTSTATUS]

		if (!n->notified || n->status != status2 ||
		    n->status.uuid != status2.uuid) 
			throw EXCEPTION("[FIRSTSTATUS] test failed");

		n->notified=false;

		mypeer q=mypeer::create("c");

		q->peerstatusupdate(nodeclusterstatus("c", x::uuid(),
						      0, false));

		if (!q->install(c).first)
			throw EXCEPTION("[REGISTERED] failed");

		if (c->getnodepeer("c").null())
			throw EXCEPTION("[GETNODEPEER] failed");

		if (n->notified)
			throw EXCEPTION("Unexpected NOTIFYSTATUS");

		if (*clusterinfoObj::status(c) != status2)
			throw EXCEPTION("Unexpected RECALCULATE");
		if (clusterinfoObj::status(c)->uuid != status2.uuid)
			throw EXCEPTION("Unexpected RECALCULATE");

		nodeclusterstatus status3("c", x::uuid(), 2, false);

		q->peerstatusupdate(status3); // [BETTERMASTER], [UUIDREPLICATE]

		if (*clusterinfoObj::status(c) != status3)
			throw EXCEPTION("[BETTERMASTER] [nodeclusterstatus:UUIDREPLICATE] test failed");

		if (clusterinfoObj::status(c)->uuid != status3.uuid ||
		    !n->notified ||
		    n->status != status3 ||
		    n->status.uuid != status3.uuid)
			throw EXCEPTION("[BETTERMASTER] [nodeclusterstatus:UUIDREPLICATE] test failed");

		if (cc->invoked != 4)
			throw EXCEPTION("[UPDATECALLBACK2] failed (2 of 3)");

		q=mypeer(); // [UUIDRESET] [UPDATECALLBACK2]

		tracker->stop_threads(false);

		if (cc->invoked != 5)
			throw EXCEPTION("[UPDATECALLBACK2] failed (3 of 3)");

		if (clusterinfoObj::status(c)->uuid == status3.uuid)
			throw EXCEPTION("[nodeclusterstatus:UUIDRESET] failed");

		p=mypeer();
	}
	// [MASTERGONE]

	tracker->stop_threads(false);
	if (!equal_sans_timestamps(*clusterinfoObj::status(c), status1))
		throw EXCEPTION("[MASTERGONE] test failed (1)");

	status1= *clusterinfoObj::status(c);

	{
		x::uuid buuid;

		mypeer p=mypeer::create("b"), q(mypeer::create("b"));

		p->peerstatusupdate(nodeclusterstatus("b", buuid, 1, false));
		q->peerstatusupdate(nodeclusterstatus("b", buuid, 1, false));

		if (!p->install(c).first ||
		    q->install(c).first) // [REPLACE]
			throw EXCEPTION("[REPLACE] test failed");

		STASHER_NAMESPACE::nodeinfomap m;

		c->update(m); // [STOP]

		if (!p->stop_received)
			throw EXCEPTION("[STOP] test failed");
	}

	tracker->stop_threads(false);
	if (!equal_sans_timestamps(*clusterinfoObj::status(c), status1))
		throw EXCEPTION("[MASTERGONE] test failed (2)");

	{
		STASHER_NAMESPACE::nodeinfomap m;

		m["b"]=STASHER_NAMESPACE::nodeinfo();
		m["c"]=STASHER_NAMESPACE::nodeinfo();

		c->update(m); // [CLUSTERUPDATE]

		x::uuid buuid;

		mypeer p=mypeer::create("b"), q=mypeer::create("c");

		p->peerstatusupdate(nodeclusterstatus("b", buuid, 1, false));

		if (!p->install(c).first ||
		    *clusterinfoObj::status(c) != nodeclusterstatus("b", buuid,
								    1, false))
			throw EXCEPTION("[MASTERUPDATE] test failed");

		if (clusterinfoObj::status(c)->uuid != buuid)
			throw EXCEPTION("[MASTERUPDATE] test failed");

		p->peerstatusupdate(nodeclusterstatus("b", buuid, 2,
						      false)); // [MASTERUPDATE]

		if (*clusterinfoObj::status(c) != nodeclusterstatus("b",
								    buuid, 2,
								    false))
			throw EXCEPTION("[MASTERUPDATE] test failed");

		if (clusterinfoObj::status(c)->uuid != buuid)
			throw EXCEPTION("[MASTERUPDATE] test failed");

		q->peerstatusupdate(nodeclusterstatus("c", x::uuid(), 1,
						      false));

		if (!q->install(c).first ||
		    *clusterinfoObj::status(c) != nodeclusterstatus("b", buuid,
								    2, false))
			throw EXCEPTION("[MASTERUPDATE] test failed");
		if (clusterinfoObj::status(c)->uuid != buuid)
			throw EXCEPTION("[MASTERUPDATE] test failed");
	}

	tracker->stop_threads(false);
	if (!equal_sans_timestamps(*clusterinfoObj::status(c), status1))
		throw EXCEPTION("[MASTERGONE] test failed (3)");

	status1= *clusterinfoObj::status(c);

	{
		mypeer p=mypeer::create("b"), q=mypeer::create("c");

		nodeclusterstatus status2("a", status1.uuid, 1, false);

		p->peerstatusupdate(status2);

		if (!p->install(c).first || // [COUNTSLAVES]
		    !equal_sans_timestamps(*clusterinfoObj::status(c), status2))
			throw EXCEPTION("[COUNTSLAVES] test failed (1)");

		if (clusterinfoObj::status(c)->uuid != status2.uuid)
			throw EXCEPTION("[COUNTSLAVES] test failed (1)");

		status2.slavecount=2;

		q->peerstatusupdate(status2);

		if (!q->install(c).first ||
		    !equal_sans_timestamps(*clusterinfoObj::status(c), status2))
			throw EXCEPTION("[COUNTSLAVES] test failed (2)");

		if (clusterinfoObj::status(c)->uuid != status2.uuid)
			throw EXCEPTION("[COUNTSLAVES] test failed (2)");
	}
}

static void test2()
{
	STASHER_NAMESPACE::stoppableThreadTrackerImpl tracker=STASHER_NAMESPACE::stoppableThreadTrackerImpl::create();

	clusterinfo c=clusterinfo::create("a", "test", tracker->getTracker(),
					  STASHER_NAMESPACE::nodeinfomap());

	{
		STASHER_NAMESPACE::nodeinfomap m;

		m["b"]=STASHER_NAMESPACE::nodeinfo();
		m["c"]=STASHER_NAMESPACE::nodeinfo();

		c->update(m);
	}

	nodeclusterstatus status1(*clusterinfoObj::status(c));

	if (!equal_sans_timestamps(status1,
				   nodeclusterstatus("a", x::uuid(), 0, false)))
		throw EXCEPTION("Empty cluster's status is something weird");

	{
		mypeer p=mypeer::create("b");

		nodeclusterstatus bstatus("b", x::uuid(), 1, false);
		p->peerstatusupdate(bstatus);

		if (!p->install(c).first ||
		    *clusterinfoObj::status(c) != bstatus)
			throw EXCEPTION("test2: failed (1)");
		if (clusterinfoObj::status(c)->uuid != bstatus.uuid)
			throw EXCEPTION("test2: failed (1)");
	}
	tracker->stop_threads(false);

	if (!equal_sans_timestamps(*clusterinfoObj::status(c), status1))
		throw EXCEPTION("Empty cluster's status is something weird");

	if (clusterinfoObj::status(c)->uuid == status1.uuid)
		throw EXCEPTION("Empty cluster's status is something weird");

	{
		status1= *clusterinfoObj::status(c);

		mypeer p=mypeer::create("b");

		p->peerstatusupdate(nodeclusterstatus("b", x::uuid(), 1, true));

		if (!p->install(c).first ||
		    !equal_sans_timestamps(*clusterinfoObj::status(c),
					   status1))
			throw EXCEPTION("test2: failed (2)");

		if (clusterinfoObj::status(c)->uuid != status1.uuid)
			throw EXCEPTION("test2: failed (2)");
	}
}

static void test3()
{
	clusterinfoptr c;
	{
		STASHER_NAMESPACE::nodeinfomap info;

		info["a"].nomaster(true);
		info["b"]=STASHER_NAMESPACE::nodeinfo();
		info["c"]=STASHER_NAMESPACE::nodeinfo();
		c=clusterinfo::create("a", "test",
				      STASHER_NAMESPACE::stoppableThreadTrackerImpl::create()
				      ->getTracker(), info);
	}

	nodeclusterstatus status1(*clusterinfoObj::status(c));

	if (!equal_sans_timestamps(status1,
				   nodeclusterstatus("a", x::uuid(), 0, true)))
		throw EXCEPTION("Empty cluster's status is something weird");

	{
		mypeer p=mypeer::create("b"), q=mypeer::create("c");

		status1.slavecount=1;

		p->peerstatusupdate(status1);

		if (!p->install(c).first ||
		    !equal_sans_timestamps(*clusterinfoObj::status(c), status1))
			throw EXCEPTION("test3: failed (1)");

		if (clusterinfoObj::status(c)->uuid != status1.uuid)
			throw EXCEPTION("test3: failed (1)");


		q->peerstatusupdate(nodeclusterstatus("c", x::uuid(), 0, true));

		if (!q->install(c).first ||
		    !equal_sans_timestamps(*clusterinfoObj::status(c), status1))
			throw EXCEPTION("test3: failed (2)");

		if (clusterinfoObj::status(c)->uuid != status1.uuid)
			throw EXCEPTION("test3: failed (2)");

		nodeclusterstatus cstatus("c", x::uuid(), 0, false);

		q->peerstatusupdate(cstatus);

		if (!equal_sans_timestamps(*clusterinfoObj::status(c), cstatus))
			throw EXCEPTION("test3: failed (3)");

		if (clusterinfoObj::status(c)->uuid != cstatus.uuid)
			throw EXCEPTION("test3: failed (3)");
	}
}

static void test4()
{
	clusterinfo c=clusterinfo::create("a", "test",
					  STASHER_NAMESPACE::stoppableThreadTrackerImpl::create()
					  ->getTracker(),
					  STASHER_NAMESPACE::nodeinfomap());

	recalculate_called_hook=false;

	STASHER_NAMESPACE::nodeinfomap m;

	m["a"]=STASHER_NAMESPACE::nodeinfo();

	c->update(m);

	if (recalculate_called_hook)
		throw EXCEPTION("test4: [CLUSTERUPDATE], test 1, failed");

	m["a"].useencryption(true);

	c->update(m);

	// [GETTHISNODE]
	{
		STASHER_NAMESPACE::nodeinfo tst;

		c->getthisnodeinfo(tst);

		if (!tst.useencryption())
			throw EXCEPTION("test4: [GETTHISNODE] failed");
	}

	if (!recalculate_called_hook)
		throw EXCEPTION("test4: [CLUSTERUPDATE], test 2, failed");

	m["b"]=STASHER_NAMESPACE::nodeinfo();

	c->update(m);

	recalculate_called_hook=false;

	c->update(m);

	if (recalculate_called_hook)
		throw EXCEPTION("test4: [CLUSTERUPDATE], test 3, failed");

	m["b"].useencryption(true);

	c->update(m);

	if (!recalculate_called_hook)
		throw EXCEPTION("test4: [CLUSTERUPDATE], test 3, failed");

	// [GETNODEINFO]
	{
		STASHER_NAMESPACE::nodeinfo tst;

		c->getnodeinfo("b", tst);

		if (!tst.useencryption())
			throw EXCEPTION("test4: [GETNODEINFO] failed");

		bool thrown=false;

		try {
			c->getnodeinfo("dummy", tst);
		} catch (const x::exception &e)
		{
			thrown=true;
			std::cerr << "Expected exception for [GETNODEINFO]: "
				  << e << std::endl;
		}

		if (!thrown)
			throw EXCEPTION("test4: [GETNODEINFO] failed");
	}

}

class test5_connectpeers_callback
	: public clusterinfoObj::connectpeers_callback {

public:
	std::map<std::string, x::ptr<x::obj> > *created;

	test5_connectpeers_callback(std::map<std::string, x::ptr<x::obj> >
				    *createdArg)
		: created(createdArg)
	{
	}

	~test5_connectpeers_callback()
	{
	}

	x::ptr<x::obj> operator()(const std::string &peername)
		const
	{
		x::ptr<x::obj> mcguffin(x::ptr<x::obj>::create());

		(*created)[peername]=mcguffin;
		return mcguffin;
	}
};

static void test5()
{
	clusterinfo c=clusterinfo::create("a", "test",
					  STASHER_NAMESPACE::stoppableThreadTrackerImpl::create()
					  ->getTracker(),
					  STASHER_NAMESPACE::nodeinfomap());

	{
		STASHER_NAMESPACE::nodeinfomap m;

		m["b"]=STASHER_NAMESPACE::nodeinfo();
		m["c"]=STASHER_NAMESPACE::nodeinfo();

		c->update(m);
	}

	nodeclusterstatus astatus(*clusterinfoObj::status(c));

	if (!equal_sans_timestamps(astatus,
				   nodeclusterstatus("a", x::uuid(), 0, false)))
		throw EXCEPTION("Empty cluster's status is something weird");

	mypeer pb=mypeer::create("b");

	nodeclusterstatus bstatus(nodeclusterstatus("b", x::uuid(), 1, false));
	pb->peerstatusupdate(bstatus);

	pb->initial_status_received=false;
	pb->status_update_received=false;
	pb->stop_received=false;

	if (!pb->install(c).first ||
	    !pb->initial_status_received ||
	    !pb->status_update_received ||
	    pb->stop_received)
		throw EXCEPTION("test5: node b installation failed");

	if (pb->initialstatus_value != astatus ||
	    pb->initialstatus_value.uuid != astatus.uuid)
		throw EXCEPTION("test5: did not received expected initial status");

	if (pb->statusupdate_value != bstatus ||
	    pb->statusupdate_value.uuid != bstatus.uuid)
		throw EXCEPTION("test5: did not received expected status update");

	bstatus.slavecount=2;

	pb->peerstatusupdate(bstatus);

	if (pb->statusupdate_value != bstatus ||
	    pb->statusupdate_value.uuid != bstatus.uuid)
		throw EXCEPTION("test5: did not received expected status update (2)");


	std::map<std::string, x::ptr<x::obj> > conn_attempts_1;

	c->connectpeers( test5_connectpeers_callback(&conn_attempts_1) );

	if (conn_attempts_1.size() != 1 ||
	    conn_attempts_1.begin()->first != "c") // [CONNECTPEERS]
		throw EXCEPTION("[CONNECTPEERS] failed");

	std::map<std::string, x::ptr<x::obj> > conn_attempts_2;

	c->connectpeers( test5_connectpeers_callback(&conn_attempts_2) );

	if (conn_attempts_2.size() != 0) // [RECONNECTPEERS]
		throw EXCEPTION("[RECONNECTPEERS] failed (1 of 2)");

	conn_attempts_1.clear();

	c->connectpeers( test5_connectpeers_callback(&conn_attempts_2) );

	if (conn_attempts_2.size() != 1 ||
	    conn_attempts_2.begin()->first != "c") // [CONNECTPEERS]
		throw EXCEPTION("[RECONNECTPEERS] failed (2 of 2)");

	{
		STASHER_NAMESPACE::nodeinfomap m;

		c->update(m);
	}

	if (!pb->stop_received)
		throw EXCEPTION("test5: did not received expected stop signal");
}

// Set up for test 6:
//
// Cluster with nodes a, b, c
//
// Test node is node "b". Connected to nodes a and c. Node c's master is node b.
// Node a's master is node a.

class test6_test {

public:
	STASHER_NAMESPACE::stoppableThreadTrackerImplptr tracker;

	clusterinfoptr c;     // Node "b"
	mypeer pa, pc;

	test6_test()
	{
		tracker=STASHER_NAMESPACE::stoppableThreadTrackerImpl::create();
		c=clusterinfo::create("b", "test",
				      tracker->getTracker(),
				      std::map<std::string,
					       STASHER_NAMESPACE::nodeinfo>());
		{
			STASHER_NAMESPACE::nodeinfomap m;

			m["a"]=STASHER_NAMESPACE::nodeinfo();
			m["c"]=STASHER_NAMESPACE::nodeinfo();

			c->update(m);
		}

		pa=mypeer::create("a");

		nodeclusterstatus astatus(nodeclusterstatus("a", x::uuid(),
							    0, false));
		pa->peerstatusupdate(astatus);

		if (!pa->install(c).first)
			throw EXCEPTION("test6: peer a install failed");

		pc=mypeer::create("c");

		nodeclusterstatus cstatus(nodeclusterstatus("c", x::uuid(),
							    1, false));

		pc->peerstatusupdate(cstatus);

		if (!pc->install(c).first)
			throw EXCEPTION("test6: peer c install failed");

		if (clusterinfoObj::status(c)->master != "c")
			throw EXCEPTION("test6: master's status not what's expected");
	}

	batonptr batonp;

	void installbaton(const char *from, const char *to)
	{
		batonp=batonptr::create(from, x::uuid(),
					to, x::uuid());

		c->installbaton(batonp);

		if (c->getbaton().null())
			throw EXCEPTION("[INSTALLBATON] failed (2)");
	}
};

static void test6()
{
	std::cout << "Testing [INSTALLBATON]" << std::endl;

	{
		test6_test t;

		{
			std::list<peerstatus> peers;

			t.c->getallpeers(peers);

			if (peers.size() != 2)
				throw EXCEPTION("getallpeers() did not return the expected 2 peers");
		}

		{
			batonptr batonp(batonptr::create("a", x::uuid(),
							 "c", x::uuid()));

			t.c->installbaton(batonp);

			if (!t.c->getbaton().null())
				throw EXCEPTION("[INSTALLBATON] failed (1)");
		}

		t.installbaton("c", "a");

		if (clusterinfoObj::status(t.c)->master != "a") // [INSTALLBATON] and [BATONFORCE] to another node
			throw EXCEPTION("[INSTALLBATON] failed (3)");
	}

	std::cout << "Testing [BATONCANCELUNKNOWNNODE]" << std::endl;
	{
		test6_test t;

		t.installbaton("c", "a");

		if (t.c->getbaton().null())
			throw EXCEPTION("Failed to install a baton");

		t.c->update(STASHER_NAMESPACE::nodeinfomap()); // [BATONCANCELUNKNOWNNODE]

		if (!t.c->getbaton().null())
			throw EXCEPTION("[BATONCANCELUNKNOWNNODE] failed");
	}

	{
		test6_test t;

		t.installbaton("c", "a");

		if (t.c->getbaton().null())
			throw EXCEPTION("Ftailed to install a baton");

		t.pc->peerstatusupdate(nodeclusterstatus("a", x::uuid(),
							 0, false));

		if (clusterinfoObj::status(t.c)->master != "a") // [BATONFORCE]
			throw EXCEPTION("[BATONFORCE] failed");

		t.pa=mypeer(); // [BATONNEWMASTERGONE]

		t.tracker->stop_threads(false);

		if (!t.c->getbaton().null())
			throw EXCEPTION("[BATONNEWMASTERGONE] failed");
		if (clusterinfoObj::status(t.c)->master != "b")
			throw EXCEPTION("After peer master disappeared, failed to become a master myself");
	}

	std::cout << "Testing [BATONMASTERISME]" << std::endl;
	{
		test6_test t;

		t.pa->peerstatusupdate(nodeclusterstatus("c", x::uuid(), 0,
							 false));

		if (clusterinfoObj::status(t.c)->master != "c")
			throw EXCEPTION("Failed to become node a's slave");

		t.installbaton("c", "b"); // [BATONMASTERISME]

		if (t.c->getbaton().null())
			throw EXCEPTION("Baton install failed");

		if (clusterinfoObj::status(t.c)->master != "b") // [BATONFORCE]
			throw EXCEPTION("[BATONMASTERISME] failed");
	}

	std::cout << "Testing [UNINSTALLBATON]" << std::endl;

	{
		test6_test t;

		t.pa->peerstatusupdate(nodeclusterstatus("d", x::uuid(), 0,
							 false));

		t.pc->peerstatusupdate(nodeclusterstatus("d", x::uuid(), 0,
							 false));


		if (clusterinfoObj::status(t.c)->master != "b")
			throw EXCEPTION("test6: master's status not what's expected (1)");

		t.pa->peerstatusupdate(nodeclusterstatus("b", x::uuid(), 0,
							 false));

		t.pc->peerstatusupdate(nodeclusterstatus("b", x::uuid(), 0,
							 false));

		if (clusterinfoObj::status(t.c)->master != "b")
			throw EXCEPTION("test6: master's status not what's expected (2)");

		t.installbaton("b", "b");

		if (clusterinfoObj::status(t.c)->master != "b")
			throw EXCEPTION("test6: master's status not what's expected (3)");

		t.pc->peerstatusupdate(nodeclusterstatus("c", x::uuid(), 1,
							 false));

		if (clusterinfoObj::status(t.c)->master != "b")
			throw EXCEPTION("test6: master's status not what's expected (4)");

		t.pa->peerstatusupdate(nodeclusterstatus("c", x::uuid(), 1,
							 false));

		if (clusterinfoObj::status(t.c)->master != "b")
			throw EXCEPTION("test6: master's status not what's expected (5)");
		t.batonp=x::ptr< batonObj>(); // [UNINSTALLBATON]

		t.tracker->stop_threads(false);
		if (clusterinfoObj::status(t.c)->master != "c")
			throw EXCEPTION("test6: master's status not what's expected (6)");


	}

}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
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
		std::cout << "test6" << std::endl;
		test6();
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

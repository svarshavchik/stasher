/*
** Copyright 2012-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "repopeerconnection.H"
#include "repocontrollerbase.H"
#include "clusterlistener.H"
#include "stoppablethreadtracker.H"
#include "trandistributor.H"
#include "spacemonitor.H"
#include "baton.H"
#include <x/serialize.H>
#include <x/options.H>
#include <x/destroycallbackflag.H>

class testconnection : public repopeerconnectionObj {

public:

	testconnection(const std::string &connectionName)
		:  repopeerconnectionObj(connectionName,
					 spacemonitor::create(x::df::create(".")))
	{
	}

	~testconnection() noexcept
	{
	}

	std::mutex mutex;
	std::condition_variable cond;
	bool flag;

	using repopeerconnectionObj::deserialized;

	void deserialized(const nodeclusterstatus &newStatus)

	{
		std::unique_lock<std::mutex> lock(mutex);

		repopeerconnectionObj::deserialized(newStatus);
		flag=true;
		cond.notify_all();
	}

	MAINLOOP_DECL;
};

MAINLOOP_IMPL(testconnection)

class testconn_wrapper {

public:
	x::ref<testconnection> conn;

	STASHER_NAMESPACE::stoppableThreadTrackerImpl tracker;

	std::pair<x::fd, x::fd> socks;

	x::fd::base::outputiter out_iter;
	x::fd::base::inputiter in_iter, in_end_iter;
	x::serialize::iterator<x::fd::base::outputiter> ser_iter;
	x::deserialize::iterator<x::fd::base::inputiter> deser_iter;

	testconn_wrapper() :
		conn(x::ref<testconnection>::create("test")),
		tracker(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create()),
		socks(x::fd::base::socketpair()),
		out_iter(socks.first),
		in_iter(socks.first),
		ser_iter(out_iter),
		deser_iter(in_iter, in_end_iter)
	{
		socks.second->nonblock(true);

		tracker->start_thread(conn,
				      socks.second,
				      x::fd::base::inputiter(socks.second),
				      tracker->getTracker(),
				      x::ptr<x::obj>::create(),
				      false,
				      x::ptr<trandistributorObj>(),
				      clusterlistenerptr(),
				      nodeclusterstatus(),
				      clusterinfoptr(),
				      tobjrepo::create("repo.tst"));

		// Wait for the initial status update
		nodeclusterstatus dummy;

		deser_iter(dummy);

	}

	void sendstatus(const nodeclusterstatus &stat)
	{
		{
			std::unique_lock<std::mutex> lock(conn->mutex);

			conn->flag=false;
		}

		ser_iter(stat);
		out_iter.flush();

		{
			std::unique_lock<std::mutex> lock(conn->mutex);

			while (!conn->flag)
				conn->cond.wait(lock);
		}
	}

	void noop()
	{
		x::ptr<x::obj> mcguffin=x::ptr<x::obj>::create();

		conn->noop(mcguffin);

		x::destroyCallbackFlag cb=x::destroyCallbackFlag::create();

		mcguffin->ondestroy([cb]{cb->destroyed();});

		mcguffin=x::ptr<x::obj>();
		cb->wait();
	}

	void statusupdated(const nodeclusterstatus &stat)
	{
		// Do it twice, because thisstatusupdated() gets invoked after
		// the write object gets queued up, so make sure that the code
		// path executes

		for (int i=0; i<2; ++i)
		{
			conn->do_statusupdated(stat);

			nodeclusterstatus dummy;

			deser_iter(dummy);
		}
	}

};

class dummyController : public repocontrollerbaseObj {

public:
	dummyController()
		: repocontrollerbaseObj("dummy", x::uuid(),
					tobjrepo::create("repo.tst"),
					repoclusterquorum::create())
	{
	}

	~dummyController() noexcept {}

	void get_quorum(const STASHER_NAMESPACE::quorumstateref &status_arg,
			const boolref &processed_arg,
			const x::ptr<x::obj> &mcguffin_arg)
	{
		static_cast<STASHER_NAMESPACE::quorumstate &>(*status_arg)=
			STASHER_NAMESPACE::quorumstate();
		processed_arg->flag=true;
	}

	start_controller_ret_t start_controller(const x::ref<x::obj> &mcguffin)
	{
		return start_controller_ret_t::create();
	}

	void handoff(const x::ptr<repocontrollerbaseObj> &next)
	{
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

static void test1()
{
	testconn_wrapper conn;

	// PEER status: nodea/uuid

	nodeclusterstatus mastera("nodea", x::uuid(), 0, false);

	conn.sendstatus(mastera);

	x::ptr<dummyController> dummy(x::ptr<dummyController>::create());
	x::ptr<x::obj> mcguffin(x::ptr<x::obj>::create());

	// Send a peerlink message with a different master name/uuid,
	// the execution thread discards it, the peerlink object gets destroyed.

	{
		x::destroyCallbackFlag cb(x::destroyCallbackFlag::create());

		auto req=repopeerconnectionbaseObj::peerlinkptr
			::create(dummy, mcguffin,
				 "nodea", x::uuid());

		conn.conn->connect_peer(req);
		req->ondestroy([cb]{cb->destroyed();});

		req=repopeerconnectionbaseObj::peerlinkptr();

		cb->wait(); // [PEERCONNECT]
	}

	// Send a peerlink with a matching master/uuid, the execution
	// thread will take it.

	x::weakptr<repopeerconnectionbaseObj::peerlinkptr> weakreq;

	{
		auto req=repopeerconnectionbaseObj::peerlinkptr
			::create(dummy, mcguffin, mastera.master, mastera.uuid);

		conn.conn->connect_peer(req);

		weakreq=req;

		// Send a peerlink with a different master/uuid, wait for
		// the execution thread to discard it, after it takes ownership
		// of the previous peerlink.

		x::destroyCallbackFlag cb(x::destroyCallbackFlag::create());

		req=repopeerconnectionbaseObj::peerlinkptr
			::create(dummy, mcguffin, "nodeb", x::uuid());

		conn.conn->connect_peer(req);
		req->ondestroy([cb]{cb->destroyed();});

		req=repopeerconnectionbaseObj::peerlinkptr();

		cb->wait();
	}

	// The execution thread should still have a reference to the first
	// peerlink.

	x::destroyCallbackFlag cb(x::destroyCallbackFlag::create());

	{
		repopeerconnectionbaseObj::peerlinkptr
			origreq(weakreq.getptr());

		if (origreq.null()) // [PEERCONNECT]
			throw EXCEPTION("[PEERCONNECT] failed");

		origreq->ondestroy([cb]{cb->destroyed();});
	}

	conn.sendstatus(nodeclusterstatus("nodeb", x::uuid(), 0, false));

	cb->wait(); //[NOLONGERPEER]

	conn.sendstatus(mastera);

	{
		mcguffin=x::ptr<x::obj>();
		cb->wait();
		mcguffin=x::ptr<x::obj>::create();

		auto req=repopeerconnectionbaseObj::peerlinkptr
			::create(dummy, mcguffin, mastera.master, mastera.uuid);

		conn.conn->connect_peer(req);

		weakreq=req;

		x::destroyCallbackFlag cb(x::destroyCallbackFlag::create());

		req=repopeerconnectionbaseObj::peerlinkptr
			::create(dummy, mcguffin, "nodeb", x::uuid());

		conn.conn->connect_peer(req);
		req->ondestroy([cb]{cb->destroyed();});

		req=repopeerconnectionbaseObj::peerlinkptr();

		cb->wait();
	}

	{
		x::destroyCallbackFlag cb(x::destroyCallbackFlag::create());


		{
			repopeerconnectionbaseObj::peerlinkptr
				link(weakreq.getptr());

			if (link.null())
				throw EXCEPTION("[CONTROLLERGONE] failed (1 of 2)");

			link->ondestroy([cb]{cb->destroyed();});

			mcguffin=x::ptr<x::obj>(); // [CONTROLLERGONE]
		}
		cb->wait();
	}

	{
		x::destroyCallbackFlag cb(x::destroyCallbackFlag::create());

		dummy->ondestroy([cb]{cb->destroyed();});

		dummy=x::ptr<dummyController>();
		cb->wait();
	}
}

static void test2()
{
	testconn_wrapper conn;

	// PEER status: nodea/uuid

	nodeclusterstatus mastera("nodea", x::uuid(), 0, false);

	conn.sendstatus(mastera);

	batonptr batonp;

	batonp=batonptr::create("nodeb", x::uuid(), "nodea", x::uuid());

	conn.conn->installformermasterbaton(batonp);

	{
		auto cb=x::destroyCallbackFlag::create();

		batonp->ondestroy([cb]{cb->destroyed();});
		batonp=batonptr();
		cb->wait(); // [INSTALLFORMERMASTERBATONIMM]
	}

	batonp=batonptr::create("nodea", x::uuid(), "nodeb", x::uuid());

	conn.conn->installformermasterbaton(batonp);

	x::weakptr<batonptr > weakbaton=batonp;

	batonp=batonptr();

	conn.sendstatus(mastera);

	if (weakbaton.getptr().null()) // [INSTALLFORMERMASTERBATON]
		throw EXCEPTION("[INSTALLFORMERMASTERBATON] failed (1)");

	conn.sendstatus(nodeclusterstatus("nodeb", x::uuid(), 0, false));

	if (!weakbaton.getptr().null()) // [INSTALLFORMERMASTERBATON]
		throw EXCEPTION("[INSTALLFORMERMASTERBATON] failed (2)");

}

static void test3()
{
	testconn_wrapper conn;

	// PEER status: nodea/uuid

	nodeclusterstatus mastera("nodea", x::uuid(), 0, false);

	conn.sendstatus(mastera);

	// NODE status: nodeb/uuid

	conn.statusupdated(nodeclusterstatus("nodeb", x::uuid(), 0, false));

	batonptr batonp;

	x::weakptr<batonptr > wbaton;

	batonp=batonptr::create("noded", x::uuid(), "nodee", x::uuid());

	conn.conn->installformermasterbaton(batonp);

	wbaton=batonp;
	batonp=batonptr();

	conn.noop();

	// PEER "test" master is nodea, NODE master nodeb
	// Baton noded->nodee: should not be held

	if (!wbaton.getptr().null())
		throw EXCEPTION("test failed (1)");

	batonp=batonptr::create("noded", x::uuid(), "test", x::uuid());

	conn.conn->installformermasterbaton(batonp);

	wbaton=batonp;
	batonp=batonptr();

	conn.noop();

	// PEER "test" master is nodea, NODE master nodeb
	// Baton noded->test: should not be held

	if (!wbaton.getptr().null())
		throw EXCEPTION("test failed (2)");


	batonp=batonptr::create("nodeb", x::uuid(), "test", x::uuid());

	conn.conn->installformermasterbaton(batonp);

	wbaton=batonp;
	batonp=batonptr();

	conn.noop();

	// PEER "test" master is nodea, NODE master nodeb
	// Baton nodeb->test: should not be held

	if (!wbaton.getptr().null())
		throw EXCEPTION("test failed (3)");

	conn.sendstatus(nodeclusterstatus("test", x::uuid(), 0, false));

	batonp=batonptr::create("nodeb", x::uuid(), "test", x::uuid());

	conn.conn->installformermasterbaton(batonp);

	wbaton=batonp;
	batonp=batonptr();

	conn.noop();

	// PEER "test" master is test, NODE master nodeb
	// Baton nodeb->test: should  be held

	if (wbaton.getptr().null())
		throw EXCEPTION("test failed (4)");

	conn.statusupdated(nodeclusterstatus("test", x::uuid(), 0, false));

	// Peer updates its status to still be its master, baton should still be held
	if (wbaton.getptr().null())
		throw EXCEPTION("test failed (5)");

	// Peer updates its status its master is nodeb, baton should be dropped

	conn.statusupdated(nodeclusterstatus("nodeb", x::uuid(), 0, false));
	conn.noop();

	if (!wbaton.getptr().null())
		throw EXCEPTION("test failed (6)");

	// PEER test, master test
	// NODE master nodeb

	conn.sendstatus(nodeclusterstatus("test", x::uuid(), 0, false));
	conn.statusupdated(nodeclusterstatus("nodeb", x::uuid(), 0, false));

	batonp=batonptr::create("nodeb", x::uuid(), "test", x::uuid());

	conn.conn->installformermasterbaton(batonp);

	wbaton=batonp;
	batonp=batonptr();
	conn.statusupdated(nodeclusterstatus("nodeb", x::uuid(), 0, false));
	conn.noop();

	// Baton should be dropped
	if (!wbaton.getptr().null())
		throw EXCEPTION("test failed (7)");

	// PEER test, master test
	// NODE master nodeb

	conn.sendstatus(nodeclusterstatus("test", x::uuid(), 0, false));
	conn.statusupdated(nodeclusterstatus("nodeb", x::uuid(), 0, false));

	batonp=batonptr::create("nodeb", x::uuid(), "test", x::uuid());

	conn.conn->installformermasterbaton(batonp);

	wbaton=batonp;
	batonp=batonptr();
	conn.statusupdated(nodeclusterstatus("test", x::uuid(), 0, false));

	conn.noop();
	// Baton should be kept
	if (wbaton.getptr().null())
		throw EXCEPTION("test failed (8)");

	conn.statusupdated(nodeclusterstatus("nodeb", x::uuid(), 0, false));
	conn.noop();
	if (!wbaton.getptr().null())
		throw EXCEPTION("test failed (9)");

	// PEER test, master test
	// NODE master nodeb

	nodeclusterstatus nodeb("nodeb", x::uuid(), 0, false);
	nodeclusterstatus test("test", x::uuid(), 0, false);

	conn.sendstatus(test);
	conn.statusupdated(nodeb);

	batonp=batonptr::create(nodeb.master, nodeb.uuid, "test", x::uuid());

	conn.conn->installformermasterbaton(batonp);

	wbaton=batonp;
	batonp=batonptr();

	conn.statusupdated(test);
	conn.noop();
	if (wbaton.getptr().null())
		throw EXCEPTION("test failed (10, part 1)");

	x::ptr<dummyController> controller(x::ptr<dummyController>::create());
	x::ptr<x::obj> controller_mcguffin(x::ptr<x::obj>::create());

	x::weakptr<repopeerconnectionbaseObj::peerlinkptr> wreq;
	{
		x::destroyCallbackFlag cb(x::destroyCallbackFlag::create());

		auto req=repopeerconnectionbaseObj::peerlinkptr
			::create(controller, controller_mcguffin,
				 nodeb.master, nodeb.uuid);

		conn.conn->connect_peer(req);

		wreq=req;
	}

	x::ptr<slavesyncinfoObj>
		syncinfo(x::ptr<slavesyncinfoObj>
			 ::create(nodeb.master,
				  nodeb.uuid,
				  x::ptr<repocontrollerslaveObj>(),
				  objrepocopydstptr(),
				  tobjrepo::create("repo.tst"),
				  boolref::create(),
				  x::ptr<x::obj>()));
	conn.conn->connect_slave(syncinfo);
	conn.statusupdated(test);
	if (!wbaton.getptr().null())
		throw EXCEPTION("test failed (10, part 2)");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		x::dir::base::rmrf("repo.tst");
		ALARM(30);

		std::cout << "test1" << std::endl;
		test1();

		std::cout << "test2" << std::endl;
		test2();

		std::cout << "test3" << std::endl;
		test3();
		x::dir::base::rmrf("repo.tst");

	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

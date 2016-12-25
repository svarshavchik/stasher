/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "objrepocopysrc.H"
#include "repocontrollermaster.H"
#include "repocontrollermasterslaveconnection.H"
#include "repopeerconnection.H"
#include "clusternotifier.H"
#include "peerstatus.H"
#include "tobjrepo.H"
#include "objrepocopydstinterface.H"
#include "objrepocopysrc.H"
#include "trandistributor.H"
#include "trancommit.H"
#include "trandistreceived.H"
#include "trandistuuid.H"
#include "baton.H"
#include <x/destroycallbackflag.H>
#include <x/threads/run.H>
#include <x/threadmsgdispatcher.H>

LOG_CLASS_INIT(repocontrollermasterObj);

// Handoff processing.

// The object that stores the metadata for the initial part of the master
// handoff processing, which involves stopping all the commit threads, and
// acquiring a write lock on the repository.

class repocontrollermasterObj::handoff_msgObj : virtual public x::obj {

public:
	std::string peername;
	bool mcguffin_destroyed;
	tobjrepoObj::commitlockptr_t commitlock;
	baton batonp;

	handoff_msgObj(const std::string &mastername,
		       const x::uuid &masteruuid,
		       const std::string &peernameArg)

		: peername(peernameArg), mcguffin_destroyed(false),
		  batonp(baton::create(mastername, masteruuid, peername,
				       x::uuid()))
	{
	}

	~handoff_msgObj() noexcept
	{
	}
};

// Internal message used by debugGetPeerConnection()

class repocontrollermasterObj::debug_get_peer_msg {
public:

	std::string peername;

	class retval : virtual public x::obj {

	public:
		repopeerconnectionptr peer;

		retval() {}
		~retval() noexcept {}
	};

	x::ptr<retval> retvalref;

	x::ptr<x::obj> mcguffin;
};

class repocontrollermasterObj::commitThreadObj : virtual public x::obj {

public:
	x::eventfd eventfd;

	commitThreadObj();
	~commitThreadObj() noexcept;

	void run(const x::ref<commitJobObj> &job,
		 const x::ref<repocontrollermasterObj> &masterArg);
};

repocontrollermasterObj::commitThreadObj
::commitThreadObj() : eventfd(x::eventfd::create())
{
}

repocontrollermasterObj::commitThreadObj::~commitThreadObj() noexcept
{
}

class repocontrollermasterObj::thisnodereceivedObj
	: public trandistreceivedObj {

public:
	// Transactions that this node received but not yet begun committing
	trandistuuid uuids;

	// Transactions that this node is in the process of committing
	tranuuid committing;

	void received(const trandistuuid &recvuuids);

	void cancelled(const tranuuid &uuids);

	thisnodereceivedObj(const repocontrollermasterptr
			    &controllerArg);
	~thisnodereceivedObj() noexcept;

private:
	x::weakptr< repocontrollermasterptr > controller;
};

repocontrollermasterObj::thisnodereceivedObj
::thisnodereceivedObj(const repocontrollermasterptr
		      &controllerArg)
	: uuids(trandistuuid::create()),
	  committing(tranuuid::create()), controller(controllerArg)
{
}

repocontrollermasterObj::thisnodereceivedObj
::~thisnodereceivedObj() noexcept
{
}

void repocontrollermasterObj::thisnodereceivedObj
::received(const trandistuuid &recvuuids)
{
	repocontrollermasterptr ptr(controller.getptr());

	if (!ptr.null())
		ptr->transactions_received(uuids, recvuuids);
}

void repocontrollermasterObj::thisnodereceivedObj
::cancelled(const tranuuid &cancuuids)
{
	repocontrollermasterptr ptr(controller.getptr());

	if (!ptr.null())
	{
		ptr->transactions_cancelled(uuids, cancuuids);
		ptr->transactions_cancelled(committing, cancuuids);
	}
}

class repocontrollermasterObj::slaveinfo {

public:

	// A mcguffin reference to a connection with a slave

	slaveConnectionptr connection;

	// Transactions the slave reports as being received

	trandistuuid received_uuids;

	// This slave is being synced

	class syncObj : virtual public x::obj {

	public:
		syncObj() : copysrc(objrepocopysrc::create()) {}
		~syncObj() noexcept {}

		// Syncing this slave
		objrepocopysrcObj::copycompleteptr copycomplete;

		// The thread that's syncing this slave
		objrepocopysrc copysrc;

		// copysrc's mcguffin
		x::weakptr<x::ptr<x::obj> > mcguffin;

		// A waiting commit lock
		tobjrepoObj::commitlockptr_t commitlock;
	};

	// A reference to a syncObj

	typedef x::ptr<syncObj> sync_t;

	//! Current sync info

	sync_t sync;

	//! Destructor callback on copysrc's mcguffin

	class syncobjcbObj : virtual public x::obj {

	public:
		x::weakptr<repocontrollermasterptr > controller;

		syncobjcbObj(const repocontrollermasterptr
			     &controllerArg)
			noexcept;
		~syncobjcbObj() noexcept;

		void destroyed();
	};

	bool synced;

	slaveinfo();
	~slaveinfo() noexcept;
};

repocontrollermasterObj::slaveinfo::slaveinfo()
	: received_uuids(trandistuuid::create()), synced(false)
{
}

repocontrollermasterObj::slaveinfo::~slaveinfo() noexcept
{
}

repocontrollermasterObj::slaveConnectionObj
::slaveConnectionObj(const repocontrollermasterptr &masterArg)
 : master(masterArg)
{
}

repocontrollermasterObj::slaveConnectionObj::~slaveConnectionObj() noexcept
{
}

void repocontrollermasterObj::slaveConnectionObj
::destroyed(const repopeerconnectionptr &dummy) noexcept
{
	// Peer connection object destroyed the peerlink object,
	// doesn't want to be my slave.
	//
	// slaveinfo::connection is now a null reference. Recompute quorum
	// status.

	LOG_DEBUG("Lost connection with a slave");

	repocontrollermasterptr ptr(master.getptr());

	if (!ptr.null())
		ptr->check_quorum();
}

// Cluster notification callback

class repocontrollermasterObj::clusterNotifierCallbackObj
	: public clusternotifierObj {

	x::weakptr<repocontrollermasterptr > controller;

public:
	clusterNotifierCallbackObj(const repocontrollermasterptr
				   &controllerArg);
	~clusterNotifierCallbackObj() noexcept;

        void clusterupdated(const clusterinfoObj::cluster_t &newStatus)
;
};

repocontrollermasterObj::clusterNotifierCallbackObj
::clusterNotifierCallbackObj(const repocontrollermasterptr
			     &controllerArg)
	: controller(controllerArg)
{
}

repocontrollermasterObj::clusterNotifierCallbackObj
::~clusterNotifierCallbackObj() noexcept
{
}

void repocontrollermasterObj::clusterNotifierCallbackObj
::clusterupdated(const clusterinfoObj::cluster_t &newStatus)

{
	repocontrollermasterptr ptr(controller.getptr());

	// New/updated list of peers in the cluster

	LOG_DEBUG("Updated cluster peer listing");

	if (!ptr.null())
		ptr->clusterupdated(newStatus);
}

repocontrollermasterObj
::repocontrollermasterObj(const std::string &masternameArg,
			  const x::uuid &masteruuidArg,
			  const tobjrepo &repoArg,
			  const repoclusterquorum &callback_listArg,
			  const x::ptr<trandistributorObj> &distributorArg,
			  const STASHER_NAMESPACE::stoppableThreadTracker &trackerArg,
			  const x::stoppable &haltstopArg)
	: repocontrollerbasehandoffObj(masternameArg,
				       masteruuidArg,
				       repoArg,
				       callback_listArg,
				       trackerArg),
	  distributor(distributorArg), haltstop(haltstopArg)
{
}

repocontrollermasterObj::~repocontrollermasterObj() noexcept
{
}

class repocontrollermasterObj::master_mcguffin : virtual public x::obj {

public:
	master_mcguffin()
	{
		LOG_TRACE("mcguffin " << dynamic_cast<x::obj *>(this)
			<< " created");
	}

	~master_mcguffin() noexcept
	{
		LOG_TRACE("mcguffin " << dynamic_cast<x::obj *>(this) << " destroyed");
	}
};

repocontrollermasterObj::start_controller_ret_t
repocontrollermasterObj::start_controller(const msgqueue_obj &msgqueue,
					  const x::ref<x::obj> &mcguffin)
{
	return tracker->start_thread(x::ref<repocontrollermasterObj>(this),
				     msgqueue,
				     mcguffin);
}

void repocontrollermasterObj::run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
				  const msgqueue_obj &msgqueue,
				  x::ref<x::obj> &start_arg)
{
	threadmsgdispatcher_mcguffin=nullptr;

	mcguffin= &start_arg; // The global mcguffin that's passed between controllers

	x::logger::context thread("master(" + mastername + ", "
				  + x::tostring(masteruuid) + ")");

	LOG_INFO("Master controller on " << mastername << " has started");

	//x::ptr<x::obj> my_mcguffin(x::ptr<x::obj>::create());
	x::ptr<x::obj> my_mcguffin(x::ptr<master_mcguffin>::create());

	clusterinfoptr clusterref;

	cluster= &clusterref;

	started();

	try {
		// Come up without a quorum, by default
		curquorum=STASHER_NAMESPACE::quorumstate();

		slaves_t slavemap;

		auto receivedref = x::ref<thisnodereceivedObj>::create
			(repocontrollermasterptr(this));

		slaves= &slavemap;
		controller_mcguffin=&my_mcguffin;
		thisnodereceived= &*receivedref;
		received_cluster_update=false;

		LOG_TRACE("Thread mcguffin for " << mastername
			  << "  " << &*my_mcguffin);

		commit_mcguffin=x::ptr<x::obj>::create();

		{
			x::ptr<trandistributorObj> d(distributor.getptr());

			if (!d.null())
			{
				LOG_DEBUG("Installed receiver for this node");
				d->installreceiver(receivedref);
			}
		}

		worker_pool_t
			commitqueueref(worker_pool_t
				       ::create(2, 8, "commit", "commitjobs"));
		commitqueue= &*commitqueueref;

		auto stop_mcguffinref=x::ref<x::obj>::create();

		x::weakptr<x::ptr<x::obj> >
			stop_mcguffinweakptr(stop_mcguffinref);

		stop_mcguffin=&stop_mcguffinweakptr;

		// There's an expectation of the initial quorum status
		// announcement. Do not disappoint.

		quorum(STASHER_NAMESPACE::quorumstate());

		while (1)
		{
			while (!(*msgqueue)->empty())
				msgqueue->event();
			check_copy_completed();

			(*msgqueue)->getEventfd()->event();
		}
	} catch (const x::stopexception &msg)
	{
		LOG_INFO("Master controller on " << mastername
			 << " is stopping");
	} catch (const x::exception &msg)
	{
		LOG_FATAL(msg);
	}
}

void repocontrollermasterObj::initialize(const clusterinfo &cluster)
{
	set_cluster(cluster);

	clusterNotifierCallback=x::ptr<clusterNotifierCallbackObj>
		::create(repocontrollermasterptr(this));

	cluster->installnotifycluster(clusterNotifierCallback);
}

void repocontrollermasterObj::set_cluster(const x::weakptr<clusterinfoptr> &cluster)
{
	do_set_cluster(cluster);
}

void repocontrollermasterObj
::dispatch_do_set_cluster(const x::weakptr<clusterinfoptr> &clusterArg)
{
	if ( (*cluster=clusterArg.getptr()).null())
		stop();
}

// Process cluster composition update
void repocontrollermasterObj::dispatch_clusterupdated(const clusterinfoObj::cluster_t &newStatus)
{
	bool received_cluster_update_already=received_cluster_update;

	received_cluster_update=true;

	for (auto b=newStatus.begin(), e=newStatus.end();
	     b != e; ++b)
	{
		if (slaves->find(b->first) == slaves->end())
		{
			slaves->insert(std::make_pair(b->first, slaveinfo()));
			LOG_INFO("Installed new peer: " << b->first);
		}
	}

	for (auto b=slaves->begin(), e=slaves->end(), p=b;
	     (p=b) != e; )
	{
		++b;

		if (newStatus.find(p->first) == newStatus.end())
		{
			LOG_INFO("Removed peer: " << p->first);
			slaves->erase(p);
		}
	}

	// The first update notification forces a quorum announcement
	compute_quorum(!received_cluster_update_already);
}

void repocontrollermasterObj::peernewmaster(const repopeerconnectionptr &peerRef,
					    const nodeclusterstatus &peerStatus)
{
	do_peernewmaster(x::weakptr<repopeerconnectionptr>(peerRef),
			 peerStatus);
}

void repocontrollermasterObj::dispatch_do_peernewmaster(const x::weakptr<repopeerconnectionptr> &peerRef,
							const nodeclusterstatus &peerStatus)
{
	repopeerconnectionptr peer(peerRef.getptr());

	if (peer.null())
	{
		LOG_TRACE("Peer object has been destroyed");
		return;
	}

	LOG_DEBUG("Peer " << peer->peername << ", status: master "
		  << peerStatus.master << ", uuid "
		  << x::tostring(peerStatus.uuid));

	if (peerStatus.master != mastername ||
	    peerStatus.uuid != masteruuid)
	{
		LOG_TRACE("Ignoring");
		return; // The peer will drop the mcguffin on its own
	}

	slaves_t::iterator iter(slaves->find(peer->peername));

	if (iter == slaves->end())
	{
		LOG_WARNING("Status from " << peer->peername
			    << ": unknown peer (" << peer->peername << ")");
		return;
	}

	// Note that a quorum is reached only when all slaves are fully
	// synced with the master. A new slave controller, on its node, comes
	// up with a non-quorum state by default, and there's no need to update
	// a new slave, since the cluster cannot be in quorum until the slave
	// gets fully synced!

	LOG_DEBUG("Connecting to " << peer->peername);

	iter->second=slaveinfo();

	// Make sure that the current state is not in quorum

	compute_quorum(false);

	auto link=repopeerconnectionbaseObj::peerlinkptr::create
		(x::ptr<repocontrollerbaseObj>(this),
		 *controller_mcguffin,
		 mastername,
		 masteruuid);

	auto conn=slaveConnection::create(repocontrollermasterptr(this));

	conn->link=link;

	// We can't install the mcguffin reference at this point. This would
	// store a strong reference to repopeerconnectionObj here. A race
	// condition may end up with the connection thread being stopped before
	// we get here, so now we store a strong repopeerconnectionObj in the
	// mcguffin reference, then send a connect_peer() message.
	//
	// Since the thread is no longer running, it never takes the peerlink
	// message off its message queue, and the peerlink object never goes
	// out of scope, and the mcguffin reference never gets nulled out,
	// and the repopeerconnectionObj object never goes out of scope.
	//
	// Rather, send the connection thread the slaveConnectionObj, which
	// upon receipt, gets bounced back to the master thread via the
	// accept() message, where the mcguffin reference gets installed.

	iter->second.connection=conn;
	peer->connect_peer(link);

	peer->connect_master(mastersyncinfo::create
			     (mastername, masteruuid,
			      repocontrollermasterptr(this),
			      conn,
			      iter->second.received_uuids));

	LOG_INFO(mastername << ": link request from " << peer->peername);
}

void repocontrollermasterObj::accept(const repopeerconnectionptr &peer,
				     const slaveConnectionptr &conn,
				     const repopeerconnectionbaseObj::peerlinkptr &link)
{
	do_accept(x::weakptr<repopeerconnectionptr>(peer),
		  x::weakptr<slaveConnectionptr>(conn),
		  x::weakptr<repopeerconnectionbaseObj::peerlinkptr>(link));
}

void repocontrollermasterObj::dispatch_do_accept(const x::weakptr<repopeerconnectionptr> &peerArg,
						 const x::weakptr<slaveConnectionptr> &connArg,
						 const x::weakptr<repopeerconnectionbaseObj::peerlinkptr> &linkArg)
{
	auto peer=peerArg.getptr();

	if (peer.null())
	{
		LOG_WARNING(mastername << ": accept: "
			    "peer connection object already destroyed");
		return;
	}

	auto conn=connArg.getptr();

	if (conn.null())
	{
		LOG_WARNING(mastername << ": accept: "
			    << peer->peername
			    << ": connection object already destroyed");
		return;
	}

	auto link=linkArg.getptr();

	if (link.null())
	{
		LOG_WARNING(mastername << ": accept: "
			    << peer->peername
			    << ": connection already disconnected from me");
		return;
	}

	LOG_INFO(mastername << ": master link established with "
		 << peer->peername);
	conn->install(peer, link);
}

void repocontrollermasterObj::dispatch_check_quorum()
{
	compute_quorum(false);
}

class repocontrollermasterObj::quorumPeerListObj
	: virtual public x::obj,
	  public std::list< std::pair<repopeerconnection,
				      slaves_t::iterator > > {

public:
	quorumPeerListObj();
	~quorumPeerListObj() noexcept;
};

repocontrollermasterObj::quorumPeerListObj::quorumPeerListObj()
{
}

repocontrollermasterObj::quorumPeerListObj::~quorumPeerListObj() noexcept
{
}

void repocontrollermasterObj::compute_quorum(bool forceannounce)
{
	if (!received_cluster_update)
		return; // Did not receive the initial cluster state yet.

	compute_quorum_get_peers(forceannounce,
				 quorumPeerList::create());
}

bool repocontrollermasterObj
::compute_quorum_get_peers(bool forceannounce,
			   const quorumPeerList &peers)
{
	STASHER_NAMESPACE::quorumstate newquorum(true, true); // Optimism

	LOG_DEBUG("Computing quorum status");

	// If there are no peers, it's always a quorum, otherwise...

	if (!slaves->empty())
	{
		size_t totpeers=0;
		size_t totslaves=0;

		for (slaves_t::iterator b(slaves->begin()), e(slaves->end());
		     b != e; ++b, ++totslaves)
		{
                        if (b->second.connection.null())
                        {
                                LOG_TRACE("Not connected with "
                                          << b->first);
                                continue;
                        }

			repopeerconnectionptr
				peer(b->second.connection->getptr());

			if (peer.null() &&
			    b->second.connection->link.getptr().null())
			{
				b->second=slaveinfo();
				LOG_WARNING("Disconnected from "
					    << b->first);
				continue;
			}

			if (!b->second.synced)
			{
				LOG_TRACE("Not synchronized with "
					  << b->first);
				continue;
			}

			peers->push_back(std::make_pair(peer, b));
			LOG_TRACE("Synchronized slave: " << b->first);
			++totpeers;
		}

		// Number of peers   Number necessary for a quorum
		// ---------------   -----------------------------
		//
		//       1                        1
		//       2                        1
		//       3                        2
		//       4                        2
		// ...

		newquorum.majority= totpeers >= (totslaves+1)/2;
		newquorum.full=totpeers == totslaves;

		LOG_DEBUG(totpeers << " out of " << totslaves << " slaves");
	}

	bool quorumChanged=false;

	if (newquorum != curquorum)
	{
		quorumChanged=true;

		LOG_TRACE("Quorum: " << x::tostring(newquorum));

		quorum(curquorum=newquorum);

		for (slaves_t::iterator b(slaves->begin()), e(slaves->end());
		     b != e; ++b)
		{
                        if (b->second.connection.null())
				continue;

			repopeerconnectionptr
				conn=b->second.connection->getptr();

			if (conn.null())
				continue;

			conn->master_quorum_announce(mastername, masteruuid,
						     curquorum);
		}
	}

	if (!quorumChanged && !forceannounce)
	{
		LOG_TRACE("No change in quorum status");
		return curquorum.majority;
	}

	// After regaining quorum, check all transactions if they can be
	// committed now.

	if (curquorum.majority)
		checkcommit(thisnodereceived->uuids, peers);

	return curquorum.majority;
}

void  repocontrollermasterObj
::get_quorum(const STASHER_NAMESPACE::quorumstateref &status,
	     const boolref &processed,
	     const x::ptr<x::obj> &mcguffin)
{
	do_get_quorum(status, processed, mcguffin);
}

void repocontrollermasterObj
::dispatch_do_get_quorum(const STASHER_NAMESPACE::quorumstateref &status,
			 const boolref &processed,
			 const x::ptr<x::obj> &mcguffin)
{
	static_cast<STASHER_NAMESPACE::quorumstate &>(*status)=curquorum;
	processed->flag=true;
}

void repocontrollermasterObj::syncslave(const x::uuid &connuuid,
					const x::weakptr<objrepocopydstinterfaceptr> &peer,
					const std::string &name,
					const batonptr &newmasterbaton,
					const x::ptr<syncslave_cbObj> &cb)
{
	do_syncslave(connuuid, peer, name, newmasterbaton, cb);
}

// Connection from a slave who wants to be synced

void repocontrollermasterObj::dispatch_do_syncslave(const x::uuid &connuuid,
						    const x::weakptr<objrepocopydstinterfaceptr> &peer,
						    const std::string &name,
						    const batonptr &newmasterbaton,
						    const x::ptr<syncslave_cbObj> &cb)
{
	LOG_DEBUG("Slave request: " << name);

	slaves_t::iterator b=slaves->find(name);

	if (b == slaves->end())
	{
		LOG_WARNING(mastername << ": sync request from unknown slave: "
			    << name);
		return;
	}

	{
		repopeerconnectionptr connptr;

		if (b->second.connection.null() ||
		    (connptr=b->second.connection->getptr()).null() ||
		    connptr->connuuid != connuuid)
		{
			LOG_WARNING(mastername
				    << ": sync request from unknown"
				    " connection to: "
				    << name);
			return;
		}
	}

	x::ptr<objrepocopydstinterfaceObj> ptr(peer.getptr());

	if (ptr.null())
	{
		LOG_WARNING(mastername
			    << ": sync request from slave that's already gone,"
			    " apparently: " << name);
		return;
	}

	if (!b->second.sync.null() &&
	    !b->second.sync->copycomplete.null())
	{
		LOG_FATAL(mastername
			  << ": internal error, duplicate sync request: "
			  << name);
		return;
	}

	slaveinfo::sync_t sync(slaveinfo::sync_t::create());

	x::ptr<x::obj> copysrc_mcguffin(x::ptr<x::obj>::create());

	{
		auto cb=x::ref<slaveinfo::syncobjcbObj>::create
			(repocontrollermasterptr(this));

		copysrc_mcguffin->ondestroy([cb]{cb->destroyed();});
	}

	LOG_DEBUG("Starting repository sync to " << name);

	if (!cb.null())
		cb->bind(sync->copysrc);

	sync->copycomplete=sync->copysrc->start(repo, ptr,
						newmasterbaton,
						copysrc_mcguffin);
	sync->mcguffin=copysrc_mcguffin;

	b->second.sync=sync;
}

repocontrollermasterObj::syncslave_cbObj::syncslave_cbObj()
{
}

repocontrollermasterObj::syncslave_cbObj::~syncslave_cbObj() noexcept
{
}

// When the source copy execution thread terminates, the mcguffin gets
// destroyed, and this callback gets invoked.

repocontrollermasterObj::slaveinfo::syncobjcbObj
::syncobjcbObj(const repocontrollermasterptr &controllerArg)
	noexcept : controller(controllerArg)
{
}

repocontrollermasterObj::slaveinfo::syncobjcbObj::~syncobjcbObj() noexcept
{
}

void repocontrollermasterObj::slaveinfo::syncobjcbObj::destroyed()
{
	repocontrollermasterptr ptr(controller.getptr());

	if (!ptr.null())
	{
		LOG_INFO("Repository copy thread terminated");
		ptr->check_repo_copy_completion();
	}
}

void repocontrollermasterObj::dispatch_check_repo_copy_completion()
{
	check_copy_completed();
}

void repocontrollermasterObj::check_copy_completed()
{
	LOG_DEBUG("Checking for repository synchronization completion");

	bool check_quorum=false;

	for (slaves_t::iterator b(slaves->begin()), e(slaves->end()); b != e;
	     ++b)
	{
		if (b->second.sync.null())
			continue;

		slaveinfo::syncObj &sync=*b->second.sync;

		if (sync.commitlock.null())
		{
			if (!sync.mcguffin.getptr().null())
				continue;
			LOG_INFO(b->first
				 << " finished syncing, acquiring a lock");
			sync.commitlock=
				repo->commitlock(get_msgqueue()->getEventfd());
		}

		// Should execute the following path after creating a new lock,
		// in order to make sure that the lock object gets pinged before
		// run() flushes the event file descriptor

		if (!sync.commitlock.null())
		{
			if (!sync.commitlock->locked())
				continue;
			LOG_INFO("Repository lock acquired for syncing of "
				 << b->first);
			sync.copycomplete->release();
			b->second.sync=slaveinfo::sync_t();
			b->second.synced=true;
			LOG_INFO(mastername << ": "
				 << b->first << " is synchronized");

			check_quorum=true;
			continue;
		}
	}

	LOG_DEBUG("Finished checking for repository synchronization completion");
	if (check_quorum)
		compute_quorum(false);
}

// ---------------------------------------------------------------------------
void repocontrollermasterObj::dispatch_transactions_received(const trandistreceived &node,
							     const trandistuuid &uuids)
{
	node->received(uuids);

	if (curquorum.majority)
		checkcommit(uuids);
}

void repocontrollermasterObj::dispatch_transactions_cancelled(const x::ptr<trancancelleduuidObj> &node,
							      const tranuuid &uuids)
{
	node->cancelled(uuids);
}

void repocontrollermasterObj::checkcommit(const trandistuuid &uuids)
{
	quorumPeerList peers=quorumPeerList::create();

	if (!compute_quorum_get_peers(false, peers))
	{
		LOG_DEBUG("Quorum not present");
		return;
	}

	checkcommit(uuids, peers);
}

class repocontrollermasterObj::commitJobObj : virtual public x::obj {

public:
	x::uuid uuid;
	dist_received_status_t status;

	quorumPeerList peers;

	x::ptr<x::obj> commit_mcguffin;

	x::weakptr<x::ptr<x::obj> > stop_mcguffin;

	commitJobObj(const x::uuid &uuidArg,
		     dist_received_status_t status,
		     const quorumPeerList &peersArg,
		     const x::ptr<x::obj> &commit_mcguffinArg,
		     x::weakptr<x::ptr<x::obj> > &stop_mcguffinArg);

	~commitJobObj() noexcept;

	void run(const x::eventfd &eventfd,
		 const x::ref<repocontrollermasterObj> &master);
};


void repocontrollermasterObj::checkcommit(const trandistuuid &uuids,
					  const quorumPeerList &peers)

{
	LOG_DEBUG("Checking for transactions that can be commited");

	if (commit_mcguffin.null())
	{
		LOG_DEBUG("Commits are held");
#ifdef DEBUG_BATON_TEST_1_HOOK_COMMIT_BLOCKED
		DEBUG_BATON_TEST_1_HOOK_COMMIT_BLOCKED();
#endif
		return;
	}

	std::map<x::uuid, dist_received_status_t> commitset;

	std::map<x::uuid, dist_received_status_t>
		&ready=thisnodereceived->uuids->uuids;

	std::set<x::uuid> &committing=thisnodereceived->committing->uuids;

	for (std::map<x::uuid, dist_received_status_t>::iterator
		     b(uuids->uuids.begin()),
		     e(uuids->uuids.end()); b != e; ++b)
	{
		std::map<x::uuid, dist_received_status_t>::iterator p=
			ready.find(b->first);

		if (p != ready.end())
		{

			LOG_TRACE("Checking if "
				  << x::tostring(b->first)
				  << " can be commited");

			commitset.insert(*p);
		}
	}

	for (auto peerEntry:*peers)
	{
		auto uuids=peerEntry.second->second.received_uuids->uuids;

		for (auto cb(commitset.begin()), ce(commitset.end()), cp=cb;
		     (cp=cb) != ce; )
		{
			++cb;

			auto found=uuids.find(cp->first);
			if (found == uuids.end())
			{
				LOG_TRACE(x::tostring(cp->first)
					  << " has not been received by "
					  << peerEntry.first->peername);
				commitset.erase(cp);
			}
			else if (found->second.sourcenode !=
				 cp->second.sourcenode)
			{
				cp->second.status=dist_received_status_err;

				LOG_FATAL(peerEntry.first->peername
					  << " says it received "
					  << x::tostring(cp->first)
					  << " from "
					  << found->second.sourcenode
					  << ", but I received it from "
					  << cp->second.sourcenode);
			}
			else if (found->second.status
				 != dist_received_status_ok)
				cp->second.status=found->second.status;
		}
	}

	for (std::map<x::uuid, dist_received_status_t>::iterator
		     b(commitset.begin()),
		     e(commitset.end()); b != e; ++b)
	{
		LOG_DEBUG("Submitting " << x::tostring(b->first)
			  << " for comitting");

		auto job=x::ref<commitJobObj>::create(b->first,
						      b->second, peers,
						      commit_mcguffin,
						      *stop_mcguffin);

		ready.erase(b->first);
		committing.insert(b->first);

		commitqueue->run(job, x::ref<repocontrollermasterObj>(this));
	}
}

repopeerconnectionptr
repocontrollermasterObj::debugGetPeerConnection(const std::string &peername)
{
	x::ptr<debug_get_peer_msg::retval> rc(x::ptr<debug_get_peer_msg::retval>
					      ::create());

	x::ptr<x::obj> mcguffin(x::ptr<x::obj>::create());

	{
		debug_get_peer_msg msg;

		msg.peername=peername;
		msg.retvalref=rc;
		msg.mcguffin=mcguffin;
		debugGetPeerConnectionImpl(msg);
	}

	// Wait for the mcguffin to go away

	auto flag=x::ref<x::destroyCallbackFlagObj>::create();

	mcguffin->ondestroy([flag]{flag->destroyed();});

	mcguffin=nullptr;
	flag->wait();

	return (rc->peer);
}

void repocontrollermasterObj::dispatch_debugGetPeerConnectionImpl(const debug_get_peer_msg &msg)
{
	slaves_t::iterator p=slaves->find(msg.peername);

	if (p != slaves->end())
		msg.retvalref->peer=p->second.connection->getptr();
}


repocontrollermasterObj::commitJobObj
::commitJobObj(const x::uuid &uuidArg,
	       dist_received_status_t statusArg,
	       const quorumPeerList &peersArg,
	       const x::ptr<x::obj> &commit_mcguffinArg,
	       x::weakptr<x::ptr<x::obj> > &stop_mcguffinArg)
	: uuid(uuidArg), status(statusArg), peers(peersArg),
	  commit_mcguffin(commit_mcguffinArg),
	  stop_mcguffin(stop_mcguffinArg)
{
}

repocontrollermasterObj::commitJobObj::~commitJobObj() noexcept
{
}

void repocontrollermasterObj::commitThreadObj
::run(const x::ref<commitJobObj> &job,
      const x::ref<repocontrollermasterObj> &masterArg)
{
	job->run(eventfd, masterArg);
}

void repocontrollermasterObj::commitJobObj
::run(const x::eventfd &eventfd,
      const x::ref<repocontrollermasterObj> &master)
{
#ifdef DEBUG_DISABLE_MASTER_COMMITS

#else
	LOG_DEBUG("Preparing to commit: " << x::tostring(uuid)
		  << " from " << status.sourcenode);

	trancommitptr tr;

	STASHER_NAMESPACE::req_stat_t res=STASHER_NAMESPACE::req_failed_stat;

	if (status.status == dist_received_status_ok)
	{
		try {
			tr=master->repo->begin_commit(uuid, eventfd);

			// Wait until the objects are locked, or until
			// the master controller wants to stop.

			auto ondestroy=({
					auto mcguffinptr=stop_mcguffin.getptr();

					if (mcguffinptr.null())
					{
						// Already gone

						LOG_TRACE("Stopping: "
							  << x::tostring(uuid));
						return;
					}

					x::ondestroy::create([eventfd]
							     {
								     eventfd->event(1);
							     },
							     mcguffinptr,
							     true);
				});
#ifdef DEBUG_MASTER_TEST_2_HOOK1
			DEBUG_MASTER_TEST_2_HOOK1();
#endif

			while (!tr->ready())
			{
				if (stop_mcguffin.getptr().null())
					return;

				eventfd->event();
			}

			LOG_TRACE("Object lock acquired: "
				  << x::tostring(uuid));

			res=tr->verify() ?
				STASHER_NAMESPACE::req_processed_stat:
				STASHER_NAMESPACE::req_rejected_stat;

		} catch (const x::exception &e)
		{
			// Could be a race condition - distributing peer
			// could've removed it.

			LOG_FATAL("begin_commit failed: " << x::tostring(uuid)
				  << ": " << e);
		}

		LOG_TRACE("Transaction verified: " << x::tostring(uuid)
			  << ", status: " << (int)res);
	}

	repopeerconnectionptr source_peer;
	x::ptr<repopeerconnectionObj::commitreqObj>
		req=x::ptr<repopeerconnectionObj::commitreqObj>
		::create(uuid, status.sourcenode, res);

	x::ptr<x::obj> mcguffin(x::ptr<x::obj>::create());

	for (auto peer : *peers)
	{
		if (peer.second->first == status.sourcenode)
		{
			source_peer=peer.first;
			continue;
		}

		LOG_TRACE("Sending commit/cancel: "
			  << x::tostring(uuid) << ", peer: "
			  << peer.second->first);

		peer.first->commit_peer(req, mcguffin);
	}

	{
		x::destroyCallbackFlag cb(x::destroyCallbackFlag::create());

		{
			x::ptr<x::obj> both_mcguffins[2]=
				{mcguffin,
				 stop_mcguffin.getptr()
				};

			if (both_mcguffins[1].null())
			{
				// Already gone

				LOG_TRACE("Stopping: " << x::tostring(uuid));
				return;
			}

			x::on_any_destroyed([cb]
					    {
						    cb->destroyed();
					    }, both_mcguffins,
					    both_mcguffins+2);
		}

		mcguffin=nullptr;

#ifdef DEBUG_MASTER_TEST_2_HOOK2
		DEBUG_MASTER_TEST_2_HOOK2();
#endif

		LOG_TRACE("Waiting for peers to process: "
			  << x::tostring(uuid));
		cb->wait();
	}

	try {
		if (res == STASHER_NAMESPACE::req_processed_stat)
		{
			LOG_TRACE("Committing on this node: "
				  << x::tostring(uuid));

			tr->commit();

		}
		else
		{
			LOG_TRACE("Rejecting on this node: "
				  << x::tostring(uuid));

			master->repo->mark_done(uuid, status.sourcenode,
						res, x::ptr<x::obj>());
		}
	} catch (const x::exception &e)
	{
		// Possible another race condition.

		LOG_FATAL("commit failed: " << x::tostring(uuid)
			  << ": " << e);
		return;
	}

	if (!source_peer.null())
	{
		LOG_TRACE("Notifying distributing peer: "
			  << x::tostring(uuid));
		source_peer->commit_peer(req, x::ptr<x::obj>());
	}
#endif
}

// ---------------------------------------------------------------------------


// Initial handoff request
x::ptr<x::obj>
repocontrollermasterObj::handoff_request(const std::string &peername)
{
	LOG_INFO(mastername << ": received handoff request to " << peername);

	auto msg=x::ref<handoff_msgObj>::create(mastername, masteruuid,
						peername);

	handoff_request_continue(msg);

	return msg->batonp; // That's the mcguffin!
}

// A helper thread that waits for the commit lock to go through
class repocontrollermasterObj::handoff_repolockthreadObj
	: public x::threadmsgdispatcherObj {

public:
	handoff_repolockthreadObj()=default;
	~handoff_repolockthreadObj() noexcept=default;

	void run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
		 const x::eventfd &eventfd,
		 const x::ref<handoff_msgObj> &msg);
};

// Destructor callback used in handoff processing

// The destructor callback invokes hanoff_request_continue() when the
// attached object gets destroyed. This gets attached to the commit thread
// mcguffin (which gets destroyed when all threads finish their job) and to the
// commit thread.

class repocontrollermasterObj::handoff_destroy_cb
	: virtual public x::obj {

public:
	x::weakptr<repocontrollermasterptr > master;
	x::ref<handoff_msgObj> msg;

	handoff_destroy_cb(const repocontrollermasterptr &masterArg,
			   const x::ref<handoff_msgObj> &msgArg)
		: master(masterArg), msg(msgArg)
	{
	}

	~handoff_destroy_cb() noexcept
	{
	}

	void destroyed()
	{
		repocontrollermasterptr ptr=master.getptr();

		if (!ptr.null())
			ptr->handoff_request_continue(msg);
	}
};

class repocontrollermasterObj::announce_mcguffin : virtual public x::obj {

public:
	batonptr batonp;

	announce_mcguffin(const batonptr &batonArg)
 : batonp(batonArg)
	{
	}

	~announce_mcguffin() noexcept
	{
		batonp->announced();
	}
};

// Initial handoff processing, in the context of the master controller thread
void repocontrollermasterObj::dispatch_handoff_request_continue(const x::ref<handoff_msgObj> &msg)
{
	if (msg->mcguffin_destroyed == false) // Initial request came in.
	{
		if (msg->peername == mastername)
		{
			LOG_INFO(mastername
				 << ": ignoring handover request to myself");
			return;
		}

		// First step. Grab the commit mcguffin, wait for the
		// commit jobs to finish

		if (commit_mcguffin.null())
		{
			LOG_ERROR("Handoff request failed: commits already blocked");
			return;
		}

		LOG_WARNING("Commit handoff: " << msg->peername);

		auto callback_lock=
			x::ref<handoff_destroy_cb>::create
			(repocontrollermasterptr(this), msg);

		msg->mcguffin_destroyed=true;

		x::ref<x::obj> mcguffin=commit_mcguffin;
		mcguffin->ondestroy([callback_lock]
				    { callback_lock->destroyed();});
		commit_mcguffin=nullptr;
		return;
	}

	if (msg->commitlock.null())
	{
		LOG_WARNING("Commit handoff, acquiring lock: "
			    << msg->peername);

		// Commits are no longer being processed, acquire a commit
		// lock on the repository.

		x::eventfd eventfd=x::eventfd::create();

		auto thr=x::ref<handoff_repolockthreadObj>::create();

		auto callback_lock=
			x::ref<handoff_destroy_cb>::create
			(repocontrollermasterptr(this), msg);
		msg->commitlock=repo->commitlock(eventfd);
		thr->ondestroy([callback_lock]{callback_lock->destroyed();});
		tracker->start_thread(thr, eventfd, msg);
		return;
	}

	// At this point we better have the commit lock. Otherwise this
	// indicates that the commit thread has stopped

	if (!msg->commitlock->locked())
	{
		dispatch_handoff_failed();
		return;
	}

#ifdef DEBUG_BATON_TEST_1_COMPLETED
	DEBUG_BATON_TEST_1_COMPLETED();
#endif

	// Commits and the repository are locked.

	// Create a baton, send an announcement to all peers.
	// A mcguffin invokes masterbaton_announced, once all peer connection
	// object has disposed of the mcguffin.

	quorumPeerList peers=quorumPeerList::create();

	compute_quorum_get_peers(false, peers);

	if (!curquorum.full)
	{
		LOG_ERROR("Full quorum not present");
		dispatch_handoff_failed();
		return;
	}

	batonptr batonp=msg->batonp;

	// Create a temporary baton that keep this node as a master, until
	// the new master is ready

	batonp->temp_baton=
		batonptr::create(mastername, masteruuid,
				 mastername, masteruuid);

	batonp->set_master_ptr(repocontrollermasterptr(this));
	batonp->set_commitlock(msg->commitlock);

	if (!(*cluster)->installbaton(batonp->temp_baton))
		return;

	auto mcguffin=x::ptr<announce_mcguffin>::create(batonp);

	for (auto peerEntry: *peers)
	{
		auto peer=peerEntry.first;

		if (peerEntry.second->first == msg->peername)
		{
			batonp->set_master_newconn(peer);
			continue;
		}
		else
		{
			peer->baton_master_announce(batonp->resigningnode,
						    batonp->resigninguuid,
						    batonp->batonuuid,
						    batonp->replacementnode,
						    mcguffin);
			batonp->push_back_master_peers(peer);
		}
	}
}

// Helper thread that waits for the commit lock to get acquired

void repocontrollermasterObj::handoff_repolockthreadObj
::run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
      const x::eventfd &eventfd,
      const x::ref<handoff_msgObj> &msg)
{
	msgqueue_auto msgqueue(this, eventfd);
	threadmsgdispatcher_mcguffin=nullptr;

	while (!msg->commitlock->locked())
	{
		if (!msgqueue->empty())
		{
			msgqueue.event();
			continue;
		}

		msgqueue->getEventfd()->event();
	}
#ifdef DEBUG_BATON_TEST_1_HOOK_LOCK_THREAD
	DEBUG_BATON_TEST_1_HOOK_LOCK_THREAD();
#endif
}

void repocontrollermasterObj::dispatch_handoff_failed()
{
	LOG_ERROR("Commit handoff failed");
	commit_mcguffin=x::ptr<x::obj>::create();
	compute_quorum(false);

#ifdef DEBUG_BATON_TEST_2_FAILED_HOOK
	DEBUG_BATON_TEST_2_FAILED_HOOK();
#endif

#ifdef DEBUG_BATON_TEST_5_FAILED_HOOK
	DEBUG_BATON_TEST_5_FAILED_HOOK();
#endif
}

void repocontrollermasterObj::dispatch_masterbaton_announced(const baton &batonp)
{
#ifdef DEBUG_BATON_TEST_2_ANNOUNCED_HOOK
	DEBUG_BATON_TEST_2_ANNOUNCED_HOOK();
#endif
#ifdef DEBUG_BATON_TEST_3_ANNOUNCED_HOOK
	DEBUG_BATON_TEST_3_ANNOUNCED_HOOK();
#endif

	batonp->send_transfer_request();
}

void repocontrollermasterObj::dispatch_masterbaton_handedover(const baton &batonp)
{
#ifdef DEBUG_BATON_TEST_4_GIVEN_CB
	DEBUG_BATON_TEST_4_GIVEN_CB();
#endif

	LOG_INFO(mastername << ": new master is up, disconnecting and holding baton until transfer is comlpete");

	(*cluster)->installformermasterbaton(batonp);

	if (!(*cluster)->installbaton(batonp))
		LOG_FATAL("Failed to install transferred baton, we're boned");

#ifdef DEBUG_BATON_TEST_6_NEWMASTER_DISCONNECT
	DEBUG_BATON_TEST_6_NEWMASTER_DISCONNECT();
#endif

}

// Once the halt status gets sent to the client, and the messages goes out of
// scope, this node can be stopped.

class repocontrollermasterObj::halt_cbObj : virtual public x::obj {

public:
	x::stoppable stop;

	halt_cbObj(const x::stoppable &stopArg) : stop(stopArg) {}
	~halt_cbObj() noexcept {}

	void destroyed()
	{
		stop->stop();
	}
};

// Wait until all pending commits are done, before proceeding with a halt.

class repocontrollermasterObj::halt_continue_cbObj
	: virtual public x::obj {

public:
	STASHER_NAMESPACE::haltrequestresults req;
	x::ref<x::obj> mcguffin;
	x::weakptr<x::ptr<repocontrollermasterObj> > master;

	halt_continue_cbObj(const STASHER_NAMESPACE::haltrequestresults &reqArg,
			    const x::ref<x::obj> &mcguffinArg,
			    const x::ptr<repocontrollermasterObj> &masterArg)
		: req(reqArg), mcguffin(mcguffinArg), master(masterArg)
	{
	}

	~halt_continue_cbObj() noexcept
	{
	}

	void destroyed()
	{
		try {
			auto m=master.getptr();

			if (!m.null())
				m->halt_continue(req, mcguffin);
		} catch (const x::exception &e)
		{
			LOG_FATAL(e);
		}
	}
};

// A request to halt the entire cluster. First step is to put all commits on
// hold, and wait for pending ones to go through.

void repocontrollermasterObj
::halt(const STASHER_NAMESPACE::haltrequestresults &req,
       const x::ref<x::obj> &mcguffin)
{
	do_halt(req, mcguffin);
}

void repocontrollermasterObj
::dispatch_do_halt(const STASHER_NAMESPACE::haltrequestresults &req,
		   const x::ref<x::obj> &mcguffinArg)
{
	if (!curquorum.full)
	{
		req->message="Cluster is not in full quorum";
		return;
	}

	// Stop all commits. When the commit mcguffin goes out of scope, any
	// pending commits are done, and we can continue with the halt.

	x::ptr<x::obj> mcguffin=commit_mcguffin;

	if (mcguffin.null())
	{
		req->message="Halt request failed: commits are already blocked";
		return;
	}

	LOG_INFO("Halt request received, stopping all commits");

	auto cb=x::ref<halt_continue_cbObj>
		::create(req, mcguffinArg,
			 x::ptr<repocontrollermasterObj>(this));
	mcguffin->ondestroy([cb]{cb->destroyed();});

	commit_mcguffin=nullptr;
}

// All commits have been finished.

void repocontrollermasterObj::dispatch_halt_continue(const STASHER_NAMESPACE::haltrequestresults &req,
						     const x::ref<x::obj> &mcguffin)
{
	// When the result message gets destroyed, stop this node.

	x::ref<halt_cbObj> cb=x::ref<halt_cbObj>::create(haltstop);
	req->ondestroy([cb]{cb->destroyed();});

	// Send a halt message to all peers. Use the connecting client's
	// supplied mcguffin as the mcguffin for all the peers. When all the
	// peers drop off, all client mcguffin's references get released.

	for (auto slave: *slaves)
	{
		repopeerconnectionptr
			peer(slave.second.connection->getptr());

		if (!peer.null())
		{
			LOG_INFO("Sending halt request to " << slave.first);
			peer->halt_request(mastername, masteruuid,
					   mcguffin);
		}
	}
	req->message="Cluster halted";
	req->halted=true;
}

std::string repocontrollermasterObj::report(std::ostream &rep)
{
	rep << "Quorum: " << x::tostring(curquorum) << std::endl
	    << "Distributor object installed: "
	    << x::tostring(!distributor.getptr().null()) << std::endl
	    << "Transactions received by this node:" << std::endl;

	thisnodereceived->uuids->toString(rep);

	rep << "Transactions being commited:" << std::endl;
	thisnodereceived->committing->toString(rep);

	for (slaves_t::iterator b=slaves->begin(), e=slaves->end();
	     b != e; ++b)
	{
		rep << "Slave " << b->first << ":" << std::endl;
		if (b->second.connection.null())
		{
			rep << "  Not connected" << std::endl;
			continue;
		}
		rep << "  Connected, synchronized: "
		    << x::tostring(b->second.synced) << std::endl;

		if (b->second.sync.null())
		{
			rep << "  Synchronization metadata not present"
			    << std::endl;
		}
		else
		{
			rep << "  Synchronization metadata:" << std::endl
			    << "    Commit lock: "
			    << (b->second.sync->commitlock.null()
				? "not present"
				: b->second.sync->commitlock->locked()
				? "locked":"not locked") << std::endl
			    << "  Repository copy object installed: "
			    << x::tostring(!b->second.sync->copycomplete.null())
			    << std::endl;
		}
		rep << "  Transactions received by this node:" << std::endl;
		b->second.received_uuids->toString(rep);
	}

	return "master(" + mastername + ", uuid" + x::tostring(masteruuid)
		+ ")";
}

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "trandistributor.H"
#include "repoclusterinfo.H"
#include "tobjrepo.H"
#include "tranmeta.H"
#include "trandistihave.H"
#include "trandistcancel.H"
#include "transerializer.H"
#include "trandistreceived.H"
#include "trancanceller.H"
#include "tranuuid.H"
#include "trandistuuid.H"
#include "repopeerconnection.H"
#include "objwriter.H"
#include "writtenobj.H"

LOG_CLASS_INIT(trandistributorObj);

#include "trandistributor.msgs.messagedef.H"
#include "trandistributor.msgs.apidef.H"

trandistributorObj::transtatusObj::transtatusObj()
	: status(STASHER_NAMESPACE::req_failed_stat)
{
}

trandistributorObj::transtatusObj::~transtatusObj() noexcept
{
}

trandistributorObj::trandistributorObj()
	: peer_list(peer_list_t::create())
{
}

trandistributorObj::~trandistributorObj() noexcept
{
}

class trandistributorObj::startarg : virtual public x::obj {

public:
	repoclusterinfo cluster;
	tobjrepo repo;
	x::ptr<x::obj> mcguffin;

	startarg(const repoclusterinfo &clusterArg,
		 const tobjrepo &repoArg,
		 const x::ptr<x::obj> &mcguffinArg)
		: cluster(clusterArg), repo(repoArg), mcguffin(mcguffinArg) {}
	~startarg() noexcept {}
};


// ----------------------------------------------------------------------------

class trandistributorObj::trandistinfo_t {

public:
	transtatus status;
	x::ptr<x::obj> mcguffin;

	trandistinfo_t(const transtatus &statusArg,
		       const x::ptr<x::obj> &mcguffinArg)
		: status(statusArg),
		  mcguffin(mcguffinArg)
	{
	}
};

x::ref<STASHER_NAMESPACE::writtenObj<transerializer > >
trandistributorObj::create_serialization_msg(const tobjrepo &repo,
					     const x::uuid &uuid)

{
	return x::ref<STASHER_NAMESPACE::writtenObj<transerializer> >
		::create(transerializer::ser(&repo, &uuid));
}


// ----------------------------------------------------------------------------

std::string trandistributorObj::getName() const
{
	return "distributor";
}


void trandistributorObj::run(const repoclusterinfo &clusterArg,
			     const tobjrepo &repoArg,
			     const x::ptr<x::obj> &mcguffin)
{
	cluster=&*clusterArg;
	repo=&*repoArg;

	trandistreceivedptr receiverrefplace;

	receiverref= &receiverrefplace;
	receiver=NULL;

	try {
		cluster->installnotifycluster(x::ptr<clusternotifierObj>(this));

		nodename=cluster->getthisnodename();

		known_nodes.clear();
		known_nodes.insert(nodename);

		x::ptr<trancancellerObj>
			canceller(new trancancellerObj(x::ptr<trandistributorObj
							      >(this),
						       nodename));

		// Install the cancel notifier, and pick up any existing
		// cancel notices that are already in the repository

		{
			tobjrepoObj::commitlock_t
				commitlock=repo->
				commitlock(msgqueue->getEventfd());

			while (!commitlock->locked())
				msgqueue->getEventfd()->event();

			for (tobjrepoObj::obj_iter_t
				     b=repo->obj_begin(tobjrepoObj::done_hier),
				     e=repo->obj_end(); b != e; ++b)
				canceller->installed(*b, x::ptr<x::obj>());

			repo->installNotifier(canceller);
		}

		trandistlist_t trandistlistref;

		trandistlist= &trandistlistref;

		load_trandistlist();
		runloop();
	} catch (const x::stopexception &dummy)
	{
		LOG_INFO("Stopped");
	} catch (const x::exception &err)
	{
		LOG_FATAL(err);
	}
}

void trandistributorObj::runloop()
{
	while (1)
		msgqueue->pop()->dispatch();
}

class trandistributorObj::purge_cb : public tobjrepoObj::finalized_cb {

public:
	std::set<x::uuid> bad_transactions;
	std::set<std::string> *known_nodes;

	purge_cb() noexcept {}
	~purge_cb() noexcept {}

	void operator()(const x::uuid &uuid,
			const dist_received_status_t &status)

	{
		if (known_nodes->find(status.sourcenode) != known_nodes->end())
			return;

		LOG_WARNING("Cancelling " << x::tostring(uuid)
			    << " from unknown node: "
			    << status.sourcenode);

		bad_transactions.insert(uuid);
	}
};

trandistributorObj::transtatus
trandistributorObj::newtransaction(const newtran &newtran,
				   const x::ptr<x::obj> &mcguffin)

{
	transtatus tstat(transtatus::create());

	submit_newtransaction(newtran, tstat, mcguffin);
	return tstat;
}

void trandistributorObj
::internal_transaction(const x::ref<internalTransactionObj> &tran)
{
	submit_internal_transaction(tran);
}

void trandistributorObj::dispatch(const submit_internal_transaction_msg &msg)
{
	msg.tran->doit();
}

void trandistributorObj::dispatch(const clusterupdated_msg &msg)

{
	dispatch_cluster_update(msg.status);
}

void trandistributorObj
::dispatch_cluster_update(const clusterinfoObj::cluster_t &newStatus)

{
	purge_cb cb;

	cb.known_nodes=&known_nodes;

	known_nodes.clear();
	known_nodes.insert(nodename);

	for (clusterinfoObj::cluster_t::const_iterator
		     b(newStatus.begin()), e(newStatus.end()); b != e; ++b)
		known_nodes.insert(b->first);

	repo->enumerate(cb);

	for (std::set<x::uuid>::iterator b=cb.bad_transactions.begin(),
		     e=cb.bad_transactions.end(); b != e; ++b)
		cancelled(*b);

}

class trandistributorObj::enumerate_ihave : public tobjrepoObj::finalized_cb {

public:
	tobjrepoObj *repo;

	std::string peername;

	trandistihave *ihave;

	enumerate_ihave() noexcept {}
	~enumerate_ihave() noexcept {}

	void operator()(const x::uuid &uuid,
			const dist_received_status_t &status)

	{
		if (status.sourcenode != peername)
			return;

		ihave->uuids.insert(uuid);
	}
};

void trandistributorObj::dispatch(const connected_msg &msg)

{
	trandistihave ihave;

	{
		enumerate_ihave e;

		e.repo=repo;
		e.peername=msg.peername;
		e.ihave= &ihave;
	
		repo->enumerate(e);

		LOG_DEBUG("Sending IHAVE to " << msg.peername);
	}

	repopeerconnectionptr connection(msg.connection.getptr());

	if (connection.null())
	{
		LOG_WARNING("dispatch(connection_msg): peer disconnected");
		return;
	}

	connection->distribute_peer(ihave);
}

void trandistributorObj::dispatch(const submit_newtransaction_msg &msg)

{
	msg.tran->getOptions()[tobjrepoObj::node_opt]=nodename;

	x::uuid msguuid(process_received(msg.tran));

	msg.status->uuid=msguuid;

	tobjrepo reporef(repo);

	trandistlist->insert(std::make_pair(msguuid,
					    trandistinfo_t(msg.status,
							   msg.mcguffin)
					    ));

	x::ref<STASHER_NAMESPACE::writtenObj<transerializer> > serializer=
		create_serialization_msg(reporef, msguuid);

	LOG_DEBUG("Distributing " << x::tostring(msguuid)
		  << " to peers");

	for (auto peerp: *peer_list)
	{
		repopeerconnectionptr peer(peerp.getptr());

		if (peer.null())
			continue;

		LOG_TRACE("Distributing " << x::tostring(msguuid)
			  << " to " << peer->peername);

		peer->distribute_peer(serializer);
	}
}

void trandistributorObj::dispatch(const canceltransaction_msg &msg)

{
	dispatch_cancel(msg.uuid);
}

void trandistributorObj::dispatch_cancel(const x::uuid &uuid)

{
	trandistcancel cancel;

	cancel.uuids.insert(uuid);

	LOG_DEBUG("Cancellation request for " << x::tostring(uuid));

	cancelled(uuid);

	for (auto peerp: *peer_list)
	{
		repopeerconnectionptr peer(peerp.getptr());

		if (peer.null())
			continue;

		LOG_TRACE("Distributing cancel " << x::tostring(uuid)
			  << " to " << peer->peername);

		peer->distribute_peer(cancel);
	}
}

// When starting, get the list of transactions that I'm distributing

void trandistributorObj::load_trandistlist()
{
	trandistihave mytrans;

	enumerate_ihave e;

	e.repo=repo;
	e.peername=nodename;
	e.ihave= &mytrans;

	repo->enumerate(e);

	for (std::set<x::uuid>::iterator b=mytrans.uuids.begin(),
		     e=mytrans.uuids.end(); b != e; ++b)
	{
		trandistlist->insert(std::make_pair(*b, trandistinfo_t
						    (transtatus::create(),
						     x::ptr<x::obj>())));
	}
}

void trandistributorObj::dispatch(const deserialized_ihave_msg &msg)

{
	repopeerconnectionptr connection(msg.connection.getptr());

	if (connection.null())
	{
		LOG_WARNING("dispatch(deserialize_ihave_msg): peer disconnected");
		return;
	}

	LOG_DEBUG("Received IHAVE from " << connection->peername);

	// Now, reconcile it with what my peer says it received from me.

	trandistcancel cancel;

	for (std::set<x::uuid>::const_iterator b(msg.msg.uuids.begin()),
		     e(msg.msg.uuids.end()); b != e; ++b)
		if (trandistlist->find(*b) == trandistlist->end())
			cancel.uuids.insert(*b);

	if (!cancel.uuids.empty())
		connection->distribute_peer(cancel);

	tobjrepo reporef(repo);

	for (trandistlist_t::iterator b=trandistlist->begin(),
		     e=trandistlist->end(); b != e; ++b)
		if (msg.msg.uuids.find(b->first) == msg.msg.uuids.end())
		{
			connection->distribute_peer
				(create_serialization_msg(reporef, b->first));
		}

	peer_list->push_back(connection);
}

void trandistributorObj::dispatch(const deserialized_cancel_msg &msg)
{
	for (std::set<x::uuid>::const_iterator b(msg.cancel.uuids.begin()),
		     e(msg.cancel.uuids.end()); b != e; ++b)
	{
		LOG_DEBUG("Cancelled " << x::tostring(*b) << " from peer");
		cancelled(*b);
	}
}

void trandistributorObj::dispatch(const deserialized_transaction_msg &msg)

{
	x::uuid uuid(process_received(msg.tran));

	LOG_DEBUG("Received " << x::tostring(uuid) << " from peer");
}

void trandistributorObj::dispatch(const deserialized_fail_msg &msg)

{
	LOG_DEBUG("Failure receiving " << x::tostring(msg.uuid)
		  << " from peer " << msg.errcode.sourcenode);

	repo->failedlist_insert(msg.uuid, msg.errcode);
	process_received(msg.uuid, msg.errcode);
}

// ----------------------------------------------------------------------------

// Enumeration callback for the initial load of transactions this node has.
//
// The distributor received a new receiver reference, so it gets all the
// transactions that the distributor knows about, right now.

class trandistributorObj::received_transactions_cb
	: public tobjrepoObj::finalized_cb {

public:
	trandistuuid uuids;
	std::map<x::uuid, dist_received_status_t> *p;

	received_transactions_cb()
	: uuids(trandistuuid::create()), p(&uuids->uuids)
	{
	}

	~received_transactions_cb() noexcept {}

	void operator()(const x::uuid &uuid,
			const dist_received_status_t &status)

	{
		p->insert(std::make_pair(uuid, status));
	}
};

void trandistributorObj::dispatch(const installreceiver_msg &msg)

{
	receiver=NULL;

	*receiverref=msg.newreceiver.getptr();

	if (receiverref->null())
		return;

	receiver=&**receiverref;

	received_transactions_cb cb;

	repo->enumerate(cb);

	if (!cb.uuids->uuids.empty())
	{
#ifdef DEBUG_DISTRIBUTOR_RECEIVED_HOOK
		DEBUG_DISTRIBUTOR_RECEIVED_HOOK(cb.uuids);
#endif
		receiver->received(cb.uuids);
	}
}

x::uuid trandistributorObj::process_received(const newtran &newtran)

{
	std::string source(newtran->getOptions()[tobjrepoObj::node_opt]);
	x::uuid uuid(newtran->finalize());

	process_received(uuid, dist_received_status_t(dist_received_status_ok,
						      source));
	return uuid;
}

void trandistributorObj::process_received(const x::uuid &uuid,
					  const dist_received_status_t &status)

{
	LOG_DEBUG("Received " << x::tostring(uuid) << " from "
		  << status.sourcenode);

	if (known_nodes.find(status.sourcenode) == known_nodes.end())
	{
		LOG_WARNING("Rejecting transaction " << x::tostring(uuid)
			    << " from unknown peer " << status.sourcenode);

		if (status.status == dist_received_status_ok)
			repo->cancel(uuid);
		return;
	}

	if (receiver)
	{
		trandistuuid uuids(trandistuuid::create());

		uuids->uuids.insert(std::make_pair(uuid,
						   status));

#ifdef DEBUG_DISTRIBUTOR_RECEIVED_HOOK
		DEBUG_DISTRIBUTOR_RECEIVED_HOOK(uuids);
#endif
		receiver->received(uuids);
	}
}

void trandistributorObj::cancelled(const x::uuid &uuid)
{
	LOG_DEBUG("Cancelling " << x::tostring(uuid));

	trandistlist_t::iterator tran(trandistlist->find(uuid));

	if (tran != trandistlist->end())
	{
		STASHER_NAMESPACE::req_stat_t
			result(repo->get_tran_stat(nodename, uuid));

		LOG_TRACE("Transaction result: " << (int)result);

		tran->second.status->status=result;
		trandistlist->erase(tran);
	}

	try {
		repo->cancel(uuid);
	} catch (const x::exception &e)
	{
		LOG_ERROR(e);
	}

	if (receiver)
	{
		tranuuid uuids(tranuuid::create());

		uuids->uuids.insert(uuid);

		receiver->cancelled(uuids);
	}
}

void trandistributorObj::dispatch(const completed_msg &msg)
{
	STASHER_NAMESPACE::req_stat_t
		res=repo->get_tran_stat(nodename, msg.uuid);

	LOG_DEBUG("Transaction completed: " << x::tostring(msg.uuid)
		  << ", status=" << (int)res);

#ifdef DEBUG_DISTRIBUTOR_CANCEL_HOOK
	DEBUG_DISTRIBUTOR_CANCEL_HOOK();
#endif
	dispatch_cancel(msg.uuid);
}

std::string trandistributorObj::report(std::ostream &rep)
{
	for (auto peerp: *peer_list)
	{
		repopeerconnectionptr peer(peerp.getptr());

		if (peer.null())
			continue;

		rep << "Distributing to " << peer->peername << std::endl;
	}

	rep << std::endl;

	for (trandistlist_t::iterator b=trandistlist->begin(),
		     e=trandistlist->end(); b != e; ++b)
	{
		rep << "Distributing: " << x::tostring(b->first) << std::endl;
	}
	return "distributor(" + nodename + ")";
}


/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "repocontrollerslave.H"
#include "peerstatus.H"
#include "tobjrepo.H"
#include "objrepocopydst.H"
#include "trandistreceived.H"
#include "slavesyncinfo.H"
#include "trandistributor.H"
#include "repopeerconnection.H"

LOG_CLASS_INIT(repocontrollerslaveObj);

repocontrollerslaveObj
::repocontrollerslaveObj(const std::string &masternameArg,
			 const repopeerconnectionbase &peerArg,
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
	  peer(peerArg), synccomplete_flag(false),
	  distributor(distributorArg), haltstop(haltstopArg)
{
}

repocontrollerslaveObj::~repocontrollerslaveObj()
{
}

// Mcguffin for the repository copy thread

// When the repository copy thread terminates, invoke the controller's
// synccomplete() method.

class repocontrollerslaveObj::dstcopyObj : virtual public x::obj {

public:
	dstcopyObj();
	~dstcopyObj();

	boolref flag;

	x::weakptr<x::ptr<repocontrollerslaveObj> > me;
};

repocontrollerslaveObj::dstcopyObj::dstcopyObj()
	: flag(boolref::create())
{
}

repocontrollerslaveObj::dstcopyObj::~dstcopyObj()
{
	x::ptr<repocontrollerslaveObj> p(me.getptr());

	if (!p.null())
	{
		LOG_WARNING("Synchronization complete");
		p->synccomplete(flag);
	}
}


//---------------------------------------------------------------------------

repocontrollerslaveObj::start_controller_ret_t
repocontrollerslaveObj::start_controller(const msgqueue_obj &msgqueue,
					 const x::ref<x::obj> &mcguffin)
{
	return tracker->start_threadmsgdispatcher(x::ref<repocontrollerslaveObj>(this),
				     msgqueue,
				     mcguffin);
}

void repocontrollerslaveObj::run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
				 const msgqueue_obj &msgqueue,
				 x::ref<x::obj> &start_arg)
{
	threadmsgdispatcher_mcguffin=nullptr;

	mcguffin= &start_arg;

	x::logger::context thread("slave(" + mastername + ", "
				  + x::tostring(masteruuid) + ")");

	LOG_INFO("Starting sync from master: " << mastername);

	started();

	objrepocopydstptr dstcopy(objrepocopydstptr::create());

	try {
		quorum(lastmasterquorumstate=STASHER_NAMESPACE::quorumstate());

		dstcopy_ptr= &dstcopy;

		trandistreceivedptr receiver;
		receiver_ptr= &receiver;

		x::ptr<x::obj> thread_mcguffin(x::ptr<x::obj>::create());

		{
			repopeerconnectionbase peerRef(peer.getptr());

			if (peerRef.null())
			{
				LOG_WARNING("Master peer no longer exists");

				// Must wait until we get a handoff message
			}
			else
				initialize(peerRef, thread_mcguffin);
		}

		while (1)
		{
			msgqueue->event();
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

void repocontrollerslaveObj::initialize(const repopeerconnectionbase &peerRef,
					const x::ptr<x::obj> &thread_mcguffin)

{
	peerRef->connect_peer(repopeerconnectionbaseObj::peerlinkptr
			      ::create(x::ptr<repocontrollerbaseObj>(this),
				       thread_mcguffin, mastername,
				       masteruuid));

	x::ptr<dstcopyObj> dco(x::ptr<dstcopyObj>::create());

	x::ptr<repocontrollerslaveObj> me(this);

	dco->me=me;

	auto handle=slavesyncinfo::create(mastername, masteruuid,
					  me, *dstcopy_ptr, repo,
					  dco->flag, dco);

	peerRef->connect_slave(handle);
}

void repocontrollerslaveObj::dispatch_synccomplete(const boolref &flag)
{
	std::lock_guard<std::mutex> lock(objmutex);

	*dstcopy_ptr=objrepocopydstptr(); // Must be done in thread context

	if ((synccomplete_flag=flag->flag) == true)
	{
#ifdef DEBUG_SYNCCOPY_COMPLETED
		DEBUG_SYNCCOPY_COMPLETED();
#endif
		LOG_INFO("Master synchronization complete");
		quorum(lastmasterquorumstate);
	}
	else
	{
		LOG_WARNING("Master synchronization failure, awaiting doom");
	}
}

void repocontrollerslaveObj
::dispatch_installreceiver(const trandistreceived &receiver)
{
	x::ptr<trandistributorObj> d(distributor.getptr());

	if (!d.null())
	{
		LOG_DEBUG("Installed receiver for this node");
		*receiver_ptr=receiver;
		d->installreceiver(receiver);
	}
}

void repocontrollerslaveObj::get_quorum(const STASHER_NAMESPACE::quorumstateref
					&status,
					const boolref &processed,
					const x::ptr<x::obj> &mcguffin)
{
	do_get_quorum(status, processed, mcguffin);
}

void repocontrollerslaveObj
::dispatch_do_get_quorum(const STASHER_NAMESPACE::quorumstateref &status,
			 const boolref &processed,
			 const x::ptr<x::obj> &mcguffin)
{
	static_cast<STASHER_NAMESPACE::quorumstate &>(*status)=inquorum();
	processed->flag=true;
}

void repocontrollerslaveObj
::peernewmaster(const repopeerconnectionptr &peerRef,
		const nodeclusterstatus &peerStatus)
{
}

void repocontrollerslaveObj
::dispatch_master_quorum_announce(const STASHER_NAMESPACE::quorumstate &status)
{
	LOG_INFO("Master " << mastername << " quorum status: "
		 << x::tostring(status)
		 << ", sync completed: " << x::tostring(synccomplete_flag));
	lastmasterquorumstate=status;

	if (synccomplete_flag)
		quorum(lastmasterquorumstate);
}

x::ptr<x::obj>
repocontrollerslaveObj::handoff_request(const std::string &peername)

{
	x::ptr<x::obj> mcguffin(x::ptr<x::obj>::create());

	master_handoff_request(peername, mcguffin);
	return mcguffin;
}

void repocontrollerslaveObj
::dispatch_master_handoff_request(const std::string &newmastername,
				  const x::ptr<x::obj> &mcguffin)
{
	LOG_INFO("Request to " << mastername << " to hand off to "
		 << newmastername);

	repopeerconnectionbase ptr=peer.getptr();

	if (ptr.null())
		return;

	repopeerconnectionObj *conn=
		dynamic_cast<repopeerconnectionObj *>(&*ptr);

	conn->master_handover_request(newmastername, mcguffin);
}

void repocontrollerslaveObj::halt(const STASHER_NAMESPACE::haltrequestresults
				  &req,
				  const x::ref<x::obj> &mcguffin)
{
	req->message="Cannot halt the cluster, the cluster must be halted by "
		+ mastername;
}

std::string repocontrollerslaveObj::report(std::ostream &rep)

{
	rep << "Connection object installed: "
	    << x::tostring(!peer.getptr().null()) << std::endl

	    << "Destination repo copy object installed: "
	    << x::tostring(!dstcopy_ptr->null()) << std::endl

	    << "Receiver object installed: "
	    << x::tostring(!receiver_ptr->null()) << std::endl

	    << "Synchronized with master: "
	    << x::tostring(synccomplete_flag) << std::endl
	    << "Distributor object installed: "
	    << x::tostring(!distributor.getptr().null()) << std::endl;

	return "slave(" + mastername + ", uuid" + x::tostring(masteruuid)
		+ ")";
}

/*
** Copyright 2012-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "repopeerconnection.H"
#include "repocontrollermaster.H"
#include "repoclusterinfo.H"
#include "nodeclusterstatus.H"
#include "trandistributor.H"
#include "trandistuuid.H"
#include "clusterlistener.H"
#include "spacemonitor.H"
#include "baton.H"

#include <x/logger.H>

#include <sstream>


#define IS_LINKED(masternameArg, masteruuidArg)			\
	(!currentpeerlink->null() &&				\
	 (*currentpeerlink)->mastername == (masternameArg) &&	\
	 (*currentpeerlink)->masteruuid == (masteruuidArg))

#define IS_SLAVE(masternameArg, masteruuidArg)				\
	(slavemetaptr->installed() && IS_LINKED(masternameArg, masteruuidArg))

#define IS_MASTER(masternameArg, masteruuidArg)				\
	(mastermetaptr->installed() && IS_LINKED(masternameArg, masteruuidArg))

LOG_CLASS_INIT(repopeerconnectionObj);

MAINLOOP_IMPL(repopeerconnectionObj)

#include "repopeerconnection.msgs.def.H"

x::property::value<bool>
repopeerconnectionObj::debugnotimeout("debugnotimeout", false);

// Thread that acquires the commitlock and hands off the baton back to the
// connection thread for the current master.

class repopeerconnectionObj::baton_slave_commitlock_thread
	: public x::eventqueuemsgdispatcherObj {

	LOG_CLASS_SCOPE;

public:
	baton batonp;

	x::weakptr<repopeerconnectionptr > slavethread;

	// If this baton is formed on the new master, the cluster where
	// the baton should be installed, after it's sent to the distthreads;

	x::weakptr<clusterinfoptr > baton_clusterinfo;

	baton_slave_commitlock_thread(const x::eventfd &eventfdArg,
				      const baton &batonArg,
				      const repopeerconnectionptr
				      &slavethreadArg,
				      const clusterinfoptr &clusterArg)
		: x::eventqueuemsgdispatcherObj(eventfdArg),
		  batonp(batonArg),
		  slavethread(slavethreadArg),
		  baton_clusterinfo(clusterArg)
	{
	}

	~baton_slave_commitlock_thread() noexcept
	{
	}

	void run();
};

LOG_CLASS_INIT(repopeerconnectionObj::baton_slave_commitlock_thread);

//! Repository copy source interface when syncing from a master

//! When syncing from a master, this repository copy source interface
//! is connected to the destination interface created by the
//! slave controller.
//!
//! The source interface writes the messages to the peer.

class repopeerconnectionObj::srcmasterObj : public objrepocopysrcinterfaceObj {

	//! The writer thread
	x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj> writer;
public:
	//! Constructor
	srcmasterObj(const x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj> &writerArg)
;

	//! Destructor
	~srcmasterObj() noexcept;

	//! Process a batonresponse message
	void event(const objrepocopy::batonresponse &msg);

	//! Process a slavelist message
	void event(const objrepocopy::slavelist &msg);

	//! Process a slavelistready message

	void event(const objrepocopy::slavelistready &msg);

	//! Process a slavelistdone message
	void event(const objrepocopy::slavelistdone &msg);
};

class repopeerconnectionObj::slavemeta {

public:
	//! Destination repository
	tobjrepoptr dstrepo;

	//! Destination repository copy interface
	objrepocopydstptr dstptr;

	//! The connected source repo
	x::ptr<srcmasterObj> srcmaster;

	//! The controller object
	x::weakptr<x::ptr<repocontrollerslaveObj> > slave;

	//! Announce transactions received by this node to this node's master.

	class tranreceivednotifyObj : public trandistreceivedObj {

	public:
		//! Master's name
		std::string mastername;

		//! Master's uuid
		x::uuid masteruuid;

		//! Writer thread
		x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj> writer;

		//! Constructor
		tranreceivednotifyObj(const std::string &masternameArg,
				      const x::uuid &masteruuidArg,
				      const x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj>
				      &writerArg);

		//! Destructor
		~tranreceivednotifyObj() noexcept;

		//! This node received transaction(s)
		void received(const trandistuuid &uuids);

		//! This node received a request to cancel transaction(s)
		void cancelled(const tranuuid &uuids);
	};

	//! The received transactions notifier instance

	//! The instance gets instantiated upon connection to a master,
	//! but does not get installed until the master finishes synchronizing
	//! this node's repository.
	x::ptr<tranreceivednotifyObj> receivednotify;

	//! An indication whether this node is connected to a slave controller
	bool installed();

	//! This node's master finished synchronizing this node's repository

	//! copycomplete() clears everything in this node, except for dstrepo.
	void copycomplete();

	//! Constructor
	slavemeta() noexcept;

	//! Destructor
	~slavemeta() noexcept;

	//! Baton received from master, for handing off to another

	batonptr slave_baton;
};

repopeerconnectionObj::srcmasterObj
::srcmasterObj(const x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj> &writerArg)
	: writer(writerArg)
{
}

repopeerconnectionObj::srcmasterObj::~srcmasterObj() noexcept
{
}

void repopeerconnectionObj::srcmasterObj
::event(const objrepocopy::batonresponse &msg)

{
	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<objrepocopy::batonresponse> >::create(msg));
}

void repopeerconnectionObj::srcmasterObj
::event(const objrepocopy::slavelist &msg)

{
	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<objrepocopy::slavelist> >::create(msg));
}

void repopeerconnectionObj::srcmasterObj
::event(const objrepocopy::slavelistready &msg)

{
	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<objrepocopy::slavelistready> >
		      ::create(msg));
}

void repopeerconnectionObj::srcmasterObj
::event(const objrepocopy::slavelistdone &msg)

{
	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<objrepocopy::slavelistdone> >
		      ::create(msg));
}

//----------------------------------------------------------------------------

//! Repo copy destination interface when syncing a slave

//! When syncing a slave, this repository copy destination interface
//! is connected to the source interface created by the master controller.
//!
//! The destination interface writes the messages to the peer.

class repopeerconnectionObj::dstmasterObj : public objrepocopydstinterfaceObj {

	//! The writer thread
	x::ptr<STASHER_NAMESPACE::fdobjwriterthreadObj> writer;

	//! The peer connection thread

	x::weakptr<repopeerconnectionptr > peerthread;

	//! Whether a copycomplete message has been received.
	bool copycomplete_received;

	//! The source interface from the master controller

	x::weakptr<objrepocopysrcinterfaceptr> src;
public:


	//! Constructor
	dstmasterObj(const x::ptr<STASHER_NAMESPACE::fdobjwriterthreadObj> &writerArg,
		     const repopeerconnectionptr &peerthreadArg);

	//! Destructor
	~dstmasterObj() noexcept;

	//! Process a batonrequest message
	void event(const objrepocopy::batonrequest &msg);

	//! Process a masterlist message
	void event(const objrepocopy::masterlist &msg);

	//! Process a masterlistdone message
	void event(const objrepocopy::masterlistdone &msg);

	//! Process a slaveliststart message
	void event(const objrepocopy::slaveliststart &msg);

	//! Process a masterack message
	void event(const objrepocopy::masterack &msg);

	//! Process a copycomplete message
	void event(const objrepocopy::copycomplete &msg);

	//! Process an objserializer message
	void event(const objserializer &msg);

	//! Return an indication whether the copycomplete message has been sent
	bool completed();

	//! Bind to the master controller's source interface
	void bind(const objrepocopysrcinterfaceptr &srcArg);

	//! Get the bound source interface
	objrepocopysrcinterfaceptr getsrc();
};

repopeerconnectionObj::dstmasterObj
::dstmasterObj(const x::ptr<STASHER_NAMESPACE::fdobjwriterthreadObj> &writerArg,
	       const repopeerconnectionptr &peerthreadArg)
	: writer(writerArg), peerthread(peerthreadArg)
{
}

repopeerconnectionObj::dstmasterObj::~dstmasterObj() noexcept
{
}

void repopeerconnectionObj::dstmasterObj
::event(const objrepocopy::batonrequest &msg)

{
	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<objrepocopy::batonrequest> >
		      ::create(msg));
}

void repopeerconnectionObj::dstmasterObj
::event(const objrepocopy::masterlist &msg)

{
	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<objrepocopy::masterlist> >
		      ::create(msg));
}

void repopeerconnectionObj::dstmasterObj
::event(const objrepocopy::masterlistdone &msg)

{
	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<objrepocopy::masterlistdone> >
		      ::create(msg));
}

void repopeerconnectionObj::dstmasterObj
::event(const objrepocopy::slaveliststart &msg)

{
	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<objrepocopy::slaveliststart> >
		      ::create(msg));
}

void repopeerconnectionObj::dstmasterObj
::event(const objrepocopy::masterack &msg)

{
	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<objrepocopy::masterack> >
		      ::create(msg));
}

void repopeerconnectionObj::dstmasterObj
::event(const objrepocopy::copycomplete &msg)

{
	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<objrepocopy::copycomplete> >
		      ::create(msg));

	{
		std::lock_guard<std::mutex> lock(objmutex);

		copycomplete_received=true;
	}

	repopeerconnectionptr ptr(peerthread.getptr());

	if (!ptr.null())
		ptr->check_sync_end();
}

void repopeerconnectionObj::dstmasterObj
::event(const objserializer &msg)

{
	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<objserializer> >::create(msg));
}

bool repopeerconnectionObj::dstmasterObj::completed()
{
	std::lock_guard<std::mutex> lock(objmutex);

	return copycomplete_received;
}

void repopeerconnectionObj::dstmasterObj
::bind(const objrepocopysrcinterfaceptr &srcArg)
{
	std::lock_guard<std::mutex> lock(objmutex);

	src=srcArg;
}

objrepocopysrcinterfaceptr repopeerconnectionObj::dstmasterObj::getsrc()
{
	std::lock_guard<std::mutex> lock(objmutex);

	return src.getptr();
}

//! Helper class - bind controller's source interface to the peer's destination interface

class repopeerconnectionObj::syncslave_cbObj
	: public repocontrollermasterObj::syncslave_cbObj {

	//! Peer's name, for debugging purposes
	std::string peername;

public:
	//! The destination interface

	x::weakptr<x::ptr<dstmasterObj> > dst;

	//! Constructor
	syncslave_cbObj(const x::ptr<dstmasterObj> &dstArg,
			const std::string &peernameArg);

	//! Destructor
	~syncslave_cbObj() noexcept;

	//! Bind to this source interface
	void bind(const objrepocopysrcinterfaceptr &src)
;
};

repopeerconnectionObj::syncslave_cbObj
::syncslave_cbObj(const x::ptr<dstmasterObj> &dstArg,
		  const std::string &peernameArg)
	: peername(peernameArg), dst(dstArg)
{
}

repopeerconnectionObj::syncslave_cbObj::~syncslave_cbObj() noexcept
{
}

void repopeerconnectionObj::syncslave_cbObj
::bind(const objrepocopysrcinterfaceptr &src)

{
	x::ptr<dstmasterObj> ptr(dst.getptr());

	if (!ptr.null())
	{
		LOG_INFO("Started repository copy to " << peername);
		ptr->bind(src);
	}
}

//----------------------------------------------------------------------------

repopeerconnectionObj::objdeserializer
::objdeserializer(repopeerconnectionObj &conn)

	: objserializer(conn.slavemetaptr->dstrepo,
			conn.peername)
{
}

repopeerconnectionObj::objdeserializer::~objdeserializer() noexcept
{
}

repopeerconnectionObj::slavesyncrequest::slavesyncrequest()
{
}

repopeerconnectionObj::slavesyncrequest::~slavesyncrequest() noexcept
{
}


repopeerconnectionObj::slavemeta::slavemeta() noexcept
{
}

repopeerconnectionObj::slavemeta::~slavemeta() noexcept
{
}

bool repopeerconnectionObj::slavemeta::installed()
{
	return !dstrepo.null();
}

void repopeerconnectionObj::slavemeta::copycomplete()
{
	slavemeta dummy;

	dummy.dstrepo=dstrepo;

	*this=dummy;
}

repopeerconnectionObj::slavemeta::tranreceivednotifyObj
::tranreceivednotifyObj(const std::string &masternameArg,
			const x::uuid &masteruuidArg,
			const x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj>
			&writerArg)
	: mastername(masternameArg),
	  masteruuid(masteruuidArg),
	  writer(writerArg)
{
}

repopeerconnectionObj::slavemeta::tranreceivednotifyObj
::~tranreceivednotifyObj() noexcept
{
}

void repopeerconnectionObj::slavemeta::tranreceivednotifyObj
::received(const trandistuuid &uuids)
{
	tranrecvcanc msg(mastername, masteruuid);

	msg.received->uuids=uuids->uuids;

	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<tranrecvcanc> >::create(msg));
}

void repopeerconnectionObj::slavemeta::tranreceivednotifyObj
::cancelled(const tranuuid &uuids)
{
	tranrecvcanc msg(mastername, masteruuid);

	msg.cancelled->uuids=uuids->uuids;

	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<tranrecvcanc> >::create(msg));
}

class repopeerconnectionObj::mastermeta {

public:
	mastermeta();
	~mastermeta() noexcept;

	repocontrollermasterptr master;
	x::ptr<dstmasterObj> dstcopy;
	batonptr newmasterbaton;

	std::map<x::uuid, x::ptr<x::obj> > pending_commits;

	trandistuuid peertrans;

	bool dstcopy_done;

	x::ptr<x::obj> baton_announce_mcguffin;
	x::ptr<x::obj> halt_mcguffin;

	bool installed()
	{
		return !master.null();
	}
};

repopeerconnectionObj::mastermeta::mastermeta()
	: peertrans(trandistuuid::create()), dstcopy_done(false)
{
}

repopeerconnectionObj::mastermeta::~mastermeta() noexcept
{
}

LOG_FUNC_SCOPE_DECL(repopeerconnectionObj::disconnect, disconnect_log);

void repopeerconnectionObj::disconnect()
{
	LOG_FUNC_SCOPE(disconnect_log);

	if (!currentpeerlink->null())
		LOG_WARNING(getthisnodename()
			    << ": Peer connection to controller broken: "
			    "master "
			    << (*currentpeerlink)->mastername
			    << " ("
			    << x::tostring((*currentpeerlink)->masteruuid)
			    << ")");

	*currentconnect=peerdisconnect_msg();
	*currentpeerlink=peerlinkptr();
	*slavemetaptr=slavemeta();
	*mastermetaptr=mastermeta();
}

//----------------------------------------------------------------------------

repopeerconnectionObj::trandeserializer
::trandeserializer(repopeerconnectionObj &conn)
 : transerializer(tobjrepo(conn.distrepo), conn.df)
{
}

repopeerconnectionObj::trandeserializer::~trandeserializer() noexcept
{
}

// ---------------------------------------------------------------------------

repopeerconnectionObj::repopeerconnectionObj(const std::string &connectionName,
					     const spacemonitor
					     &dfArg)

	: peerstatusannouncerObj(connectionName), df(dfArg),
	  slavesyncrequest_received(false)
{
}

repopeerconnectionObj::~repopeerconnectionObj() noexcept
{
}

LOG_FUNC_SCOPE_DECL(repopeerconnectionObj::dispatch::connect::peer,
		    connect_peerLog);

void repopeerconnectionObj::dispatch(const connect_peer_msg &msg)

{
	LOG_FUNC_SCOPE(connect_peerLog);

	peerstatusObj::status thisstatus(this);

	if (thisstatus->master != msg.peerlinkArg->mastername ||
	    thisstatus->uuid != msg.peerlinkArg->masteruuid)
	{
		LOG_WARNING(getthisnodename()
			    << ": Failed controller connection request with"
			    " peer: master "
			    << msg.peerlinkArg->mastername
			    << " ("
			    << x::tostring(msg.peerlinkArg->masteruuid)
			    << "), peer's master is "
			    << thisstatus->master
			    << " ("
			    << x::tostring(thisstatus->uuid)
			    << ")");
		return;
	}

	x::ptr<repocontrollerbaseObj>
		controllerptr(msg.peerlinkArg->controller.getptr());

	if (controllerptr.null())
	{
		LOG_WARNING(getthisnodename()
			    << ": Null controller object: master "
			    << msg.peerlinkArg->mastername
			    << " ("
			    << x::tostring(msg.peerlinkArg->masteruuid)
			    << ")");
		return;
	}

	x::ptr<x::obj> mcguffinptr(msg.peerlinkArg->controller_mcguffin.getptr());

	if (mcguffinptr.null())
	{
		LOG_WARNING(getthisnodename()
			    <<": Null controller object trigger: master "
			    << msg.peerlinkArg->mastername
			    << " ("
			    << x::tostring(msg.peerlinkArg->masteruuid)
			    << ")");
		return;
	}

	disconnect(); // For a good measure

	auto newdisconnect=peerdisconnect_msg
		::create(x::ptr<repopeerconnectionbaseObj>(this));

	newdisconnect->install(controllerptr, mcguffinptr);

	*currentpeerlink=msg.peerlinkArg;
	*currentconnect=newdisconnect;

	LOG_INFO(getthisnodename()
		 << ": Connected controller with peer: master "
		 << msg.peerlinkArg->mastername
		 << " ("
		 << x::tostring(msg.peerlinkArg->masteruuid)
		 << ")");
}

void repopeerconnectionObj::dispatch(const disconnect_peer_msg &dummy)

{
	LOG_DEBUG("Connection to " << peername
		  << " on " << getthisnodename()
		  << " received a disconnect message");

	if (currentconnect->null())
	{
		LOG_TRACE("Not currently connected to a controller");
		return;
	}

	if ( (*currentconnect)->getptr().null())
	{
		LOG_FUNC_SCOPE(disconnect_log);

		LOG_WARNING(getthisnodename() << ": controller  with "
			    << peername << " has disconnected");
		disconnect();
	}
}

LOG_FUNC_SCOPE_DECL(repopeerconnectionObj::dispatch::connect::slave,
		    connect_slaveLog);

void repopeerconnectionObj::dispatch(const connect_slave_msg &msg)

{
	LOG_FUNC_SCOPE(connect_slaveLog);

	LOG_TRACE(getthisnodename() << ": received connect_slave message to "
		  << peername);

	// Grab the pending baton, if there is one.

	batonptr syncbaton= *pendingslavebaton;
	*pendingslavebaton=batonptr();
	*pendingmasterbaton=batonptr();

	if (currentpeerlink->null())
	{
		LOG_WARNING(getthisnodename()
			    << ": connection with " << peername
			    << ": ignoring slave synchandle for "
			    << msg.synchandle->mastername
			    << " ("
			    << x::tostring(msg.synchandle->masteruuid)
			    << "), not connected to this controller");
		return;
	}

	if (msg.synchandle->mastername != thisstatus.master ||
	    msg.synchandle->masteruuid != thisstatus.uuid)
	{
		LOG_WARNING(getthisnodename()
			    << ": connection with " << peername
			    << ": ignoring slave synchandle for "
			    << msg.synchandle->mastername
			    << " ("
			    << x::tostring(msg.synchandle->masteruuid)
			    << "), current status announced to peer is "
			    << thisstatus.master
			    << " ("
			    << x::tostring(thisstatus.uuid)
			    << ")");
		return;
	}

	LOG_INFO(getthisnodename()
		 << ": connection with " << peername
		 << ": slave connection established with: "
		 << msg.synchandle->mastername
		 << " ("
		 << x::tostring(msg.synchandle->masteruuid) << ")");

	objrepocopydstptr dstptr(msg.synchandle->dstinterface.getptr());

	if (dstptr.null())
	{
		LOG_WARNING(getthisnodename()
			    << ": connection with " << peername
			    << ": Destination sync interface object"
			    " no longer available");
		return;
	}

	auto srcmaster=
		x::ref<srcmasterObj>::create(x::ref<STASHER_NAMESPACE
					     ::fdobjwriterthreadObj>
					     (writer));

	if (slavemetaptr->installed() || mastermetaptr->installed())
		throw EXCEPTION("Internal error - peer already linked");

	dstptr->start(msg.synchandle->repo,
		      srcmaster,
		      msg.synchandle->dstflag,
		      syncbaton,
		      msg.synchandle->dstmcguffin);

	slavemetaptr->dstrepo=msg.synchandle->repo;
	slavemetaptr->dstptr=dstptr;
	slavemetaptr->srcmaster=srcmaster;
	slavemetaptr->slave=msg.synchandle->controller;

	slavemetaptr->receivednotify=
		x::ptr<repopeerconnectionObj::slavemeta::tranreceivednotifyObj>
		::create(msg.synchandle->mastername,
			 msg.synchandle->masteruuid,
			 x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj>(writer));

	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<slavesyncrequest> >
		      ::create(slavesyncrequest()));
}

LOG_FUNC_SCOPE_DECL(repopeerconnectionObj::dispatch::connect::master,
		    connect_masterLog);

void repopeerconnectionObj::dispatch(const connect_master_msg &msg)
{
	LOG_FUNC_SCOPE(connect_masterLog);

	batonptr syncbaton= *pendingmasterbaton;
	*pendingslavebaton=batonptr();
	*pendingmasterbaton=batonptr();

	if (currentpeerlink->null())
	{
		LOG_WARNING(getthisnodename()
			    << ": connection with " << peername
			    << ": ignoring master synchandle for "
			    << msg.synchandle->mastername
			    << " ("
			    << x::tostring(msg.synchandle->masteruuid)
			    << "), not connected to this controller");
		return;
	}

	if (msg.synchandle->mastername != thisstatus.master ||
	    msg.synchandle->masteruuid != thisstatus.uuid)
	{
		LOG_WARNING(getthisnodename()
			    << ": connection with " << peername
			    << ": ignoring master synchandle for "
			    << msg.synchandle->mastername
			    << " ("
			    << x::tostring(msg.synchandle->masteruuid)
			    << "), current status announced to peer is "
			    << thisstatus.master
			    << " ("
			    << x::tostring(thisstatus.uuid)
			    << ")");
		return;
	}

	LOG_INFO(getthisnodename()
		 << ": connection with " << peername
		 << ": master connection established with "
		 << msg.synchandle->mastername
		 << " ("
		 << x::tostring(msg.synchandle->masteruuid) << ")");

	repocontrollermasterptr ptr(msg.synchandle->controller.getptr());

	if (ptr.null())
	{
		LOG_WARNING("Master object no longer available");
		return;
	}

	if (mastermetaptr->installed() || slavemetaptr->installed())
		throw EXCEPTION("Internal error - peer already linked");

	mastermetaptr->master=ptr;
	mastermetaptr->peertrans=msg.synchandle->received_uuids;

	if (!(mastermetaptr->newmasterbaton=syncbaton).null())
		LOG_INFO(getthisnodename()
			 << ": connection with " << peername
			 << ": installed the baton that was being held for the"
			 " new master");

	ptr->accept(repopeerconnectionptr(this),
		    msg.synchandle->connection, *currentpeerlink);

	check_sync_start();
}

void repopeerconnectionObj::deserialized(const nodeclusterstatus &newStatus)

{
	LOG_INFO(getthisnodename()
		 << ": Peer " << peername << " status changed: master "
		 << newStatus.master
		 << " ("
		 << x::tostring(newStatus.uuid) << ")");

#ifdef DEBUG_BATON_TEST_4_CLUSTERSTATUS_HOOK
	DEBUG_BATON_TEST_4_CLUSTERSTATUS_HOOK();
#endif

	LOG_DEBUG(getthisnodename() << " updated by peer " << peername
		  << ", status changed: master "
		  << newStatus.master
		  << " ("
		  << x::tostring(newStatus.uuid) << ")");

	{
		peerstatusObj::status curstatus(this);

		if (curstatus->master != newStatus.master ||
		    curstatus->uuid != newStatus.uuid)
		{
			LOG_INFO(getthisnodename() <<
				 " discarding sync request from slave " <<
				 peername
				 << ", status changed: master "
				 << newStatus.master
				 << " ("
				 << x::tostring(newStatus.uuid) << ")");

			slavesyncrequest_received=false;
		}
	}

        if (!currentpeerlink->null() &&
            ((*currentpeerlink)->mastername != newStatus.master ||
             (*currentpeerlink)->masteruuid != newStatus.uuid))
	{
		if (slavemetaptr->installed() &&
		    !slavemetaptr->slave_baton.null())
			checknewmaster();

		LOG_FUNC_SCOPE(disconnect_log);

		LOG_WARNING(getthisnodename()
			    << ": Peer " << peername
			    << " status changed: master "
			    << newStatus.master
			    << " ("
			    << x::tostring(newStatus.uuid)
			    << "): disconnect from controller: "
			    << (*currentpeerlink)->mastername
			    << " ("
			    << x::tostring((*currentpeerlink)->masteruuid)
			    << ")");

		disconnect();
	}

	LOG_DEBUG(getthisnodename() << ": registering new status of "
		  << peername);
	peerstatusannouncerObj::deserialized(newStatus);
	checkformermasterbaton(newStatus);
}

void repopeerconnectionObj::checknewmaster()
{
	// Check if the new master is up

	clusterinfo cluster=getthiscluster();

	std::string newmastername=slavemetaptr->slave_baton->replacementnode;
	peerstatus peer=cluster->getnodepeer(newmastername);

	if (peer.null())
		return;

	if (peerstatusObj::status(peer)->master != newmastername)
		return;

	LOG_INFO(getthisnodename()
		 << ": connection with " << peername
		 << ": disconnecting from old master, connecting to new master");

	cluster->installformermasterbaton(slavemetaptr->slave_baton);
	cluster->installbaton(slavemetaptr->slave_baton);
#ifdef DEBUG_BATON_TEST_6_SLAVE_FORMERMASTER_DISCONNECT
	DEBUG_BATON_TEST_6_SLAVE_FORMERMASTER_DISCONNECT();
#endif
}

void repopeerconnectionObj::dispatch(const installformermasterbaton_msg &msg)

{
	LOG_INFO("On " << getthisnodename()
		 << ", connection with "
		 << peername
		 << " is holding a baton from "
		 << msg.batonp->resigningnode);

	*formermasterbaton=msg.batonp;

	nodeclusterstatus peer_status=*peerstatusObj::status(this);

	// Maybe the peer already switched over
	checkformermasterbaton(peer_status);

	if (msg.batonp->replacementnode == peername &&
	    peer_status.master == peername)
	{
		LOG_INFO("On " << getthisnodename()
			 << ", connection with "
			 << peername
			 << " is holding a baton from "
			 << msg.batonp->resigningnode
			 << " pending enslavement");

		*pendingslavebaton=msg.batonp;
	}
}

void repopeerconnectionObj::thisstatusupdated()
{
	if (pendingslavebaton->null())
		return;

	if (thisstatus.master != peername)
	{
		LOG_INFO("On " << getthisnodename()
			 << ", connection with "
			 << peername
			 << " is dropping the pending baton because this node's master is now "
			 << thisstatus.master);
		*pendingslavebaton=batonptr();
	}

	nodeclusterstatus peer_status=*peerstatusObj::status(this);

	checkformermasterbaton(peer_status);
}

void repopeerconnectionObj::checkformermasterbaton(const nodeclusterstatus
						   &peerstatus)

{
	if (formermasterbaton->null() ||
	    (*formermasterbaton)->resigningnode == peerstatus.master)
	{
		checkpendingmasterbaton(peerstatus);
		return;
	}

	LOG_INFO("On " << getthisnodename()
		 << ", connection with "
		 << peername
		 << " is releasing the baton from "
		 << (*formermasterbaton)->resigningnode
		 << " because the peer's master is "
		 << peerstatus.master);

	*pendingmasterbaton=*formermasterbaton;
	*formermasterbaton=batonptr();
	checkpendingmasterbaton(peerstatus);
}

void repopeerconnectionObj::checkpendingmasterbaton(const nodeclusterstatus
						    &peerstatus)

{
	if (pendingmasterbaton->null())
		return;

	std::string moi=getthisnodename();

	if ((*pendingmasterbaton)->replacementnode != moi)
	{
		LOG_INFO("On " << getthisnodename()
			 << ", connection with "
			 << peername
			 << " is releasing the baton to "
			 << (*pendingmasterbaton)->replacementnode
			 << " because it's not it");
	}
	else if (peerstatus.master != moi)
	{
		LOG_INFO("On " << getthisnodename()
			 << ", connection with "
			 << peername
			 << " is releasing the baton to "
			 << (*pendingmasterbaton)->replacementnode
			 << " because peer's master is " << peerstatus.master);

	}
	else if (thisstatus.master != moi)
	{
		LOG_INFO("On " << getthisnodename()
			 << ", connection with "
			 << peername
			 << " is releasing the baton to "
			 << (*pendingmasterbaton)->replacementnode
			 << " because this node's master is "
			 << thisstatus.master);
	}
	else
		return;

	*pendingmasterbaton=batonptr();
}

void repopeerconnectionObj::deserialized(const objrepocopy::batonrequest &msg)

{
	if (!slavemetaptr->dstptr.null())
		slavemetaptr->dstptr->event(msg);
}

void repopeerconnectionObj::deserialized(const objrepocopy::masterlist &msg)

{
	if (!slavemetaptr->dstptr.null())
		slavemetaptr->dstptr->event(msg);
}

void repopeerconnectionObj::deserialized(const objrepocopy::masterlistdone &msg)

{
	if (!slavemetaptr->dstptr.null())
		slavemetaptr->dstptr->event(msg);
}

void repopeerconnectionObj::deserialized(const objrepocopy::slaveliststart &msg)

{
	if (!slavemetaptr->dstptr.null())
		slavemetaptr->dstptr->event(msg);
}

void repopeerconnectionObj::deserialized(const objrepocopy::masterack &msg)

{
	if (!slavemetaptr->dstptr.null())
		slavemetaptr->dstptr->event(msg);
}

void repopeerconnectionObj::deserialized(const objrepocopy::copycomplete &msg)

{
	if (!slavemetaptr->dstptr.null())
		slavemetaptr->dstptr->event(msg);

	x::ptr<repocontrollerslaveObj> ptr(slavemetaptr->slave.getptr());

	if (ptr.null())
	{
		LOG_WARNING("Slave controller object no longer available");
	}
	else
	{
		ptr->installreceiver(slavemetaptr->receivednotify);
	}

	slavemetaptr->copycomplete();
}

void repopeerconnectionObj::deserialized(const objdeserializer &msg)

{
	// The magic has already happened as part of the deserialization
	// process.
}

void repopeerconnectionObj::deserialized(const slavesyncrequest &msg)

{
	LOG_INFO(getthisnodename() << " received sync request from "
		 << peername << ", status: master "
		 << ({
				 std::ostringstream o;

				 peerstatusObj::status peerstatus(this);

				 o << peerstatus->master << "("
				   << x::tostring(peerstatus->uuid)
				   << ")";

				 o.str(); }));

	slavesyncrequest_received=true;
	check_sync_start();
}

void repopeerconnectionObj::deserialized(const objrepocopy::batonresponse &msg)

{
	if (!mastermetaptr->dstcopy.null())
	{
		objrepocopysrcinterfaceptr
			src(mastermetaptr->dstcopy->getsrc());

		if (!src.null())
			src->event(msg);
	}
}

void repopeerconnectionObj::deserialized(const objrepocopy::slavelist &msg)

{
	if (!mastermetaptr->dstcopy.null())
	{
		objrepocopysrcinterfaceptr
			src(mastermetaptr->dstcopy->getsrc());

		if (!src.null())
			src->event(msg);
	}
}

void repopeerconnectionObj::deserialized(const objrepocopy::slavelistready &msg)

{
	if (!mastermetaptr->dstcopy.null())
	{
		objrepocopysrcinterfaceptr
			src(mastermetaptr->dstcopy->getsrc());

		if (!src.null())
			src->event(msg);
	}
}


void repopeerconnectionObj::deserialized(const objrepocopy::slavelistdone &msg)

{
	if (!mastermetaptr->dstcopy.null())
	{
		objrepocopysrcinterfaceptr
			src(mastermetaptr->dstcopy->getsrc());

		if (!src.null())
			src->event(msg);
	}
}

void repopeerconnectionObj::deserialized(const trandistihave &msg)

{
	distributor->deserialized(msg, repopeerconnectionptr(this));
}

void repopeerconnectionObj::deserialized(const trandistcancel &msg)

{
	distributor->deserialized(msg);
}

void repopeerconnectionObj::deserialized(const transerializer &msg)

{
	if (msg.outofspace)
	{
		LOG_ERROR("No space to receive transaction "
			  << x::tostring(msg.uuid) << " from " << peername);

		distributor->deserialized_fail(msg.uuid,
					       dist_received_status_t
					       (dist_received_status_err,
						peername));
		return;
	}

	const std::map<std::string, std::string> &opts=msg.tran->getOptions();

	std::map<std::string, std::string>::const_iterator
		p=opts.find(tobjrepoObj::node_opt);

	if (p == opts.end() || p->second != peername)
	{
		LOG_FATAL(peername << " sent a transaction without identifying itsself as a source of it");
		throw EXCEPTION("Peer protocol failure");
	}

	distributor->deserialized(msg);
}

void repopeerconnectionObj::deserialized(const tranrecvcanc &msg)

{
	if (!IS_MASTER(msg.mastername, msg.masteruuid))
	{
		LOG_WARNING("Misdirected transaction announcement from "
			    << peername << " for " << msg.mastername);
		return;
	}

#ifdef DEBUG_RECEIVED_TRANS
	DEBUG_RECEIVED_TRANS(msg);
#endif

	if (!msg.received->uuids.empty())
		mastermetaptr->master
			->transactions_received(mastermetaptr->peertrans,
						msg.received);

	if (!msg.cancelled->uuids.empty())
		mastermetaptr->master
			->transactions_cancelled(mastermetaptr->peertrans,
						 msg.cancelled);
}

void repopeerconnectionObj::run(const x::fdbase &subcl_transport,
				const x::fd::base::inputiter &subcl_inputiter,
				STASHER_NAMESPACE::stoppableThreadTracker
				&trackerref,
				const x::ptr<x::obj> &subcl_mcguffin,
				bool encryptedArg,
				x::ptr<trandistributorObj> &distributorref,
				clusterlistenerptr listenerref,
				const nodeclusterstatus &peerstatusArg,
				clusterinfoptr clusterArg,
				tobjrepo &distreporef)
{
	encrypted=encryptedArg;

	// Some regression modules don't use this
	distributor=distributorref.null() ? NULL:&*distributorref;
	distrepo=&*distreporef;
	listener= listenerref.null() ? NULL: &*listenerref;

	x::weakptr<adminstopintptr> haltstopref;

	if (!clusterArg.null()) // Some regression tests leave it null
	{
		peerstatusupdate(peerstatusArg);

		installattempt(clusterArg);

		// Some regression tests don't bother to send us a stoppable one
		adminstopintObj *stoppablePtr=
			dynamic_cast<adminstopintObj *>(&*clusterArg);

		if (stoppablePtr)
			haltstopref=adminstopintptr(stoppablePtr);
	}

	haltstop= &haltstopref;

	tracker= &*trackerref;

	peerlinkptr currentpeerlinkref;
	peerdisconnect_msg currentconnectref;

	currentpeerlink= &currentpeerlinkref;
	currentconnect= &currentconnectref;

	slavemeta slavemetainstance;
	mastermeta mastermetainstance;

	slavemetaptr= &slavemetainstance;
	mastermetaptr= &mastermetainstance;

	if (distributor)
		distributor->connected(peername,
				       repopeerconnectionptr(this));

	x::ptr<x::obj> thread_mcguffinref=x::ptr<x::obj>::create();
	thread_mcguffin= &thread_mcguffinref;

	std::map<std::string, x::ptr<x::obj> > pings_outstandingref;

	pings_outstanding=&pings_outstandingref;

	batonptr pendingslavebatonref;

	pendingslavebaton= &pendingslavebatonref;

	batonptr pendingmasterbatonref;

	pendingmasterbaton= &pendingmasterbatonref;

	batonptr formermasterbatonref;

	formermasterbaton= &formermasterbatonref;

	LOG_INFO("Initialized connection with " << peername);

	if (debugnotimeout.getValue())
	{
		readTimeout_value=0;
	}

	try {
		peerstatusannouncerObj::mainloop(subcl_transport,
						 subcl_inputiter,
						 trackerref,
						 subcl_mcguffin);
	} catch (const x::stopexception &e)
	{
	} catch (const x::exception &e)
	{
		LOG_FATAL(peername << ": " << e);
		LOG_TRACE(e->backtrace);
	}
}

class repopeerconnectionObj::installattempt_cb : virtual public x::obj {

public:
	x::weakptr<repopeerconnectionptr > conn;
	x::weakptr<clusterinfoptr> cluster;

	installattempt_cb(const repopeerconnectionptr &connArg,
			  const clusterinfoptr &clusterArg)
;
	~installattempt_cb() noexcept;

	void destroyed();
};

void repopeerconnectionObj
::dispatch(const make_installattempt_msg &msg)
{
	clusterinfoptr ptr=msg.cluster.getptr();

	if (!ptr.null())
		installattempt(ptr);
}

LOG_FUNC_SCOPE_DECL(repopeerconnectionObj::installattempt, installattemptLog);

void repopeerconnectionObj
::installattempt(const clusterinfo &cluster)

{
	LOG_FUNC_SCOPE(installattemptLog);

	if (peerstatusObj::status(this)->master == cluster->getthisnodename())
	{
		LOG_WARNING(cluster->getthisnodename()
			    << ": new connection with "
			    << peername
			    << ": peer already claims I'm its master,"
			    " this must be a duplicate connection");
		stop();
		return;
	}

	LOG_DEBUG(cluster->getthisnodename()
		  << ": new connection with "
		  << peername
		  << ": installation attempt, peer status: "
		  << ({
				  peerstatusObj::status stat(this);

				  stat->master + " (" +
					  x::tostring(stat->uuid) + ")";
			  }));

	std::pair<bool, peerstatus> install_ret=install(cluster);

	if (install_ret.first)
		return;

	if (!install_ret.second.null())
	{
		// The oldest connection must be stale.

		// If the timestamps are the same, two peers
		// must be trying to connect with each other,
		// at the same time, so use the node name as
		// the tiebreaker.

		if (install_ret.second->
		    timestamp < timestamp ||

		    (install_ret.second->timestamp == timestamp &&
		     install_ret.second->connuuid < connuuid))
		{
			LOG_WARNING("Waiting for duplicate connection with "
				    << peername << " to terminate");
			x::stoppable(install_ret.second)->stop();

			auto cb=x::ref<installattempt_cb>
				::create(repopeerconnectionptr(this),
					 cluster);

			install_ret.second->ondestroy([cb]{cb->destroyed();});
			return;
		}
	}

	LOG_WARNING("Newer connection with "
		    << peername
		    << " already exists, stopping");
	stop();
}

repopeerconnectionObj::installattempt_cb
::installattempt_cb(const repopeerconnectionptr &connArg,
		    const clusterinfoptr &clusterArg)
	: conn(connArg), cluster(clusterArg)
{
}

repopeerconnectionObj::installattempt_cb::~installattempt_cb() noexcept
{
}

void repopeerconnectionObj::installattempt_cb::destroyed()
{
	repopeerconnectionptr conn_ptr(conn.getptr());
	clusterinfoptr cluster_ptr(cluster.getptr());

	if (!conn_ptr.null() && !cluster_ptr.null())
	{
		conn_ptr->make_installattempt(cluster_ptr);
		return;
	}

	LOG_WARNING("Aborting a connection installation attempt");
}

void repopeerconnectionObj::check_sync_start()
{
	if (!mastermetaptr->installed())
		return;

	if (!slavesyncrequest_received)
		return;

	if (!mastermetaptr->dstcopy.null())
		throw EXCEPTION("Internal error: master source sync was already started");

	if (mastermetaptr->dstcopy_done)
		throw EXCEPTION("Internal error: master source sync was already completed");

	batonptr newmasterbaton=mastermetaptr->newmasterbaton;

	mastermetaptr->newmasterbaton=batonptr();

	x::ptr<dstmasterObj>
		dst(x::ptr<dstmasterObj>
		    ::create(x::ptr<STASHER_NAMESPACE::fdobjwriterthreadObj>
			     (writer),
			     repopeerconnectionptr(this)));

	x::ptr<syncslave_cbObj>
		cb(x::ptr<syncslave_cbObj>::create(dst, peername));

	LOG_INFO(getthisnodename() << ": started syncing slave " << peername);

	mastermetaptr->master->syncslave(connuuid,
					 dst, peername, newmasterbaton, cb);
	mastermetaptr->dstcopy=dst;

}

void repopeerconnectionObj::dispatch(const check_sync_end_msg &dummy)

{
	if (mastermetaptr->installed() &&
	    !mastermetaptr->dstcopy.null() &&
	    mastermetaptr->dstcopy->completed())
	{
		LOG_INFO(getthisnodename()
			 << ": finished syncing slave " << peername);
		mastermetaptr->dstcopy_done=true;
		mastermetaptr->dstcopy=x::ptr<dstmasterObj>();
	}
}

void repopeerconnectionObj::dispatch(const trandistcancel &msg)

{
	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<trandistcancel> >::create(msg));
}

void repopeerconnectionObj::dispatch(const trandistihave &msg)

{
	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<trandistihave> >::create(msg));
}

void repopeerconnectionObj::dispatch(const x::ref<STASHER_NAMESPACE::writtenObj<transerializer>
						  > &msg)

{
	writer->write(msg);
}

// -------------------------------------------------------------------------

repopeerconnectionObj::commitreq::commitreq(const x::uuid &uuidArg,
					    const std::string &sourceArg,
					    STASHER_NAMESPACE::req_stat_t statusArg)
 : uuid(uuidArg), source(sourceArg),
			      status(statusArg)
{
}

repopeerconnectionObj::commitreq::commitreq()
{
}

repopeerconnectionObj::commitreq::~commitreq() noexcept
{
}

repopeerconnectionObj::commitack::commitack(const x::uuid &uuidArg)
 : uuid(uuidArg)
{
}

repopeerconnectionObj::commitack::commitack()
{
}

repopeerconnectionObj::commitack::~commitack() noexcept
{
}
repopeerconnectionObj::commitreqObj::commitreqObj(const x::uuid &uuidArg,
						  const std::string &sourceArg,
						  STASHER_NAMESPACE::req_stat_t
						  statusArg)
 : msg(uuidArg, sourceArg, statusArg)
{
}

repopeerconnectionObj::commitreqObj::~commitreqObj() noexcept
{
}

void repopeerconnectionObj::commitreqObj::serialize(STASHER_NAMESPACE::objwriterObj &writer)

{
	writer.serialize(msg);
}

void repopeerconnectionObj::deserialized(const commitreq &msg)

{
	LOG_DEBUG("Received commit request on "
		  << getthisnodename()
		  << ": " << x::tostring(msg.uuid)
		  << ", status: " << (int)msg.status);

#ifdef REPOPEERCONNECTION_DEBUG_COMMITREQ

	REPOPEERCONNECTION_DEBUG_COMMITREQ();

#endif

	if (!slavemetaptr->installed())
	{
		LOG_WARNING("Commit request for " << x::tostring(msg.uuid)
			    << " from " << peername
			    << " who is not from my master");
		return;
	}

	if (msg.status == STASHER_NAMESPACE::req_processed_stat)
	{
		slavemetaptr->dstrepo->commit_nolock(msg.uuid);
		LOG_TRACE("Commited " << x::tostring(msg.uuid));
	}
	else
	{
		slavemetaptr->dstrepo->mark_done(msg.uuid, msg.source,
						 msg.status, x::ptr<x::obj>());
		LOG_TRACE("Cancelled " << x::tostring(msg.uuid));
	}

	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<commitack> >
		      ::create(msg.uuid));

}

void repopeerconnectionObj::dispatch(const commit_peer_msg &msg)

{
	x::uuid uuid=msg.req->msg.uuid;

	LOG_DEBUG("Sending commit request: " << x::tostring(uuid)
		  << ", status: " << (int)msg.req->msg.status);

	if (!mastermetaptr->installed())
	{
		LOG_WARNING("Commit request for " << x::tostring(uuid)
			    << " to " << peername << " who is not my slave");
		return;
	}

	mastermetaptr->pending_commits.insert(std::make_pair(uuid,
							     msg.mcguffin));
	LOG_TRACE("Commit request sent");

	writer->write(msg.req);
}

void repopeerconnectionObj::deserialized(const commitack &msg)

{
	LOG_DEBUG("Received commit ack for " << x::tostring(msg.uuid));

	mastermetaptr->pending_commits.erase(msg.uuid);
}

// ----------------------------------------------------------------------------

void repopeerconnectionObj::dispatch(const baton_master_announce_msg &msg)

{
	if (!IS_MASTER(msg.mastername, msg.masteruuid))
	{
		LOG_WARNING("Ignoring baton announcement from "
			    << msg.mastername
			    << " - not currently in master mode");
		return;
	}

	mastermetaptr->baton_announce_mcguffin=msg.mcguffin;

#ifdef DEBUG_BATON_TEST_2_ANNOUNCE_HOOK
	DEBUG_BATON_TEST_2_ANNOUNCE_HOOK();
#endif

	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<baton_master_announce_msg> >
		      ::create(msg));

	LOG_INFO("Notified " << peername
		 << " that we're about to give up masterhood");
}

// Slave received the baton_master_announce_msg from its master.

// Slave creates the baton on the slave side, a commit lock on the repository,
// then hands them off to a thread that waits for the commit lock to be
// acquired.

void repopeerconnectionObj::deserialized(const baton_master_announce_msg &msg)

{
	if (!IS_SLAVE(msg.mastername, msg.masteruuid))
	{
		LOG_WARNING("Baton announce message from " << msg.mastername
			    << " who is not my master, ignoring");
		return;
	}

	LOG_INFO("Handoff baton from " << msg.mastername
		 << " to " << msg.newmasterpeer << " received");

	baton batonp(baton::create(msg.mastername,
				   msg.masteruuid,
				   msg.newmasterpeer,
				   msg.batonuuid));

	// Install a temporary baton that locks this node's status as a slave
	// to the former master

	batonp->temp_baton=batonptr::create(msg.mastername,
					    msg.masteruuid,
					    msg.mastername,
					    msg.masteruuid);

	if (!(getthiscluster()->installbaton(batonp->temp_baton)))
	{
		LOG_ERROR(getthisnodename()
			  << ": unable to install baton from "
			  << msg.mastername
			  << " to "
			  << msg.newmasterpeer);
		return;
	}

#ifdef DEBUG_BATON_TEST_3_CANCELLED_HOOK
	DEBUG_BATON_TEST_3_CANCELLED_HOOK();
#endif

	x::eventfd eventfd=x::eventfd::create();

	auto commitThread=x::ref<baton_slave_commitlock_thread>
		::create(eventfd, batonp,
			 repopeerconnectionptr(this),
			 clusterinfoptr());

	batonp->set_commitlock(slavemetaptr->dstrepo->commitlock(eventfd));

	tracker->start(commitThread);
}

class repopeerconnectionObj::baton_newmaster_installed_cb
	: virtual public x::obj {

public:
	baton batonp;

	x::weakptr<repopeerconnectionptr > master;

	baton_newmaster_installed_cb(const baton &batonArg,
				     const repopeerconnectionptr
				     &masterArg)
		: batonp(batonArg), master(masterArg)
	{
	}

	~baton_newmaster_installed_cb() noexcept
	{
		repopeerconnectionptr ptr=master.getptr();

		if (!ptr.null())
			ptr->batonismine(batonp);
	}
};

// This thread waits for the commit lock to be acquired

void repopeerconnectionObj::baton_slave_commitlock_thread::run()

{
	LOG_INFO(
		 ({
			 std::string s;

			 repopeerconnectionptr ptr=slavethread.getptr();

			 s=ptr.null() ? "Peer connection is gone":
				 ptr->getthisnodename()
				 + ": connection from " + ptr->peername
				 + ": Waiting for a commit lock";
		 }));

	while (!batonp->get_commitlock()->locked())
	{
		if (!msgqueue->empty())
		{
			msgqueue->pop()->dispatch();
			continue;
		}

		msgqueue->getEventfd()->event();
	}

#ifdef DEBUG_BATON_TEST_2_COMMITLOCK_HOOK
	DEBUG_BATON_TEST_2_COMMITLOCK_HOOK();
#endif

	repopeerconnectionptr ptr=slavethread.getptr();

	if (ptr.null())
	{
		LOG_DEBUG("Commit lock aborted: peer connection gone");
		return;
	}


	LOG_INFO(ptr->getthisnodename()
		 << ": connection from " << ptr->peername
		 << ": Commit lock acquired");

	clusterinfoptr cluster=baton_clusterinfo.getptr();

	if (!cluster.null())
	{
		LOG_INFO(ptr->getthisnodename()
			 << ": connection from " << ptr->peername
			 << ": Transfering baton to this node");

		// This node is the new master. Install the baton.

		cluster->installbaton(batonp);

		// Make sure the peers get the status update, then
		// ack with batonismine.

		auto cb=x::ref<baton_newmaster_installed_cb>::create(batonp,
								     ptr);

		cluster->pingallpeers(cb);
		return;
	}

	LOG_INFO(ptr->getthisnodename()
		 << ": connection from " << ptr->peername
		 << ": Installing baton from old master");

	ptr->baton_oldmaster_install(batonp);
}

void repopeerconnectionObj::dispatch(const baton_oldmaster_install_msg &msg)

{
	if (!IS_SLAVE(msg.batonp->resigningnode,
		      msg.batonp->resigninguuid))
	{
		LOG_ERROR("Rejected baton from "
			  << msg.batonp->resigningnode
			  << " who is not my master, "
			  << (slavemetaptr->installed() &&
			      !currentpeerlink->null()
			      ? (*currentpeerlink)->mastername
			      : "(unknown master)") << ", ignoring");
		return;
	}

#ifdef DEBUG_BATON_TEST_3_COMMITLOCK_HOOK
	DEBUG_BATON_TEST_3_COMMITLOCK_HOOK();
#endif

	baton old_baton(batonptr::create(msg.batonp->resigningnode,
					 msg.batonp->resigninguuid,
					 msg.batonp->resigningnode,
					 msg.batonp->batonuuid));

	old_baton->temp_baton=msg.batonp->temp_baton;
	msg.batonp->temp_baton=old_baton;
	slavemetaptr->slave_baton=msg.batonp;
	getthiscluster()->installbaton(old_baton);

        writer->write(x::ref<STASHER_NAMESPACE::writtenObj<baton_slave_received_msg> >::create());

}

// The master node receives the ack from the slave that it received
// the baton. The master node releases its reference on the master controller's
// handoff mcguffin.

void repopeerconnectionObj::deserialized(const baton_slave_received_msg &msg)

{
	if (!mastermetaptr->installed())
	{
		LOG_WARNING("Discarded slave baton receipt acknowledgement");
		return;
	}

	LOG_WARNING("Received slave baton receipt acknowledgement from "
		    << peername);

	mastermetaptr->baton_announce_mcguffin=x::ptr<x::obj>();
}

// Connection thread to the slave receives the handoff release notice.

void repopeerconnectionObj::dispatch(const baton_master_release_msg &msg)

{
	if (!IS_MASTER(msg.mastername, msg.masteruuid))
	{
		LOG_WARNING("Ignoring baton release from "
			    << msg.mastername
			    << " - not currently in master mode");
		return;
	}

	// Master controller's original mcguffin can be released now.

	mastermetaptr->baton_announce_mcguffin=x::ptr<x::obj>();

#ifdef DEBUG_BATON_TEST_2_RELEASE_HOOK
	DEBUG_BATON_TEST_2_RELEASE_HOOK();
#endif

	// Send the message to the slave, to release the handoff.
	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<baton_master_release_msg> >
		      ::create(msg));

	LOG_INFO("Notified " << peername
		 << " that we've decided to keep current master status");
}

// Slave received the release handoff notice from the master

void repopeerconnectionObj::deserialized(const baton_master_release_msg &msg)

{
	if (!IS_SLAVE(msg.mastername, msg.masteruuid))
	{
		LOG_WARNING("Baton announce message from " << msg.mastername
			    << " who is not my master, ignoring");
		return;
	}
#ifdef DEBUG_BATON_TEST_2_RELEASE_RECEIVED_HOOK
	DEBUG_BATON_TEST_2_RELEASE_RECEIVED_HOOK();
#endif

#ifdef DEBUG_BATON_TEST_3_RELEASE_RECEIVED_HOOK
	if (!slavemetaptr->slave_baton.null())
	{
		DEBUG_BATON_TEST_3_RELEASE_RECEIVED_HOOK();
	}
#endif
	slavemetaptr->slave_baton=batonptr();
}

class repopeerconnectionObj::baton_given_cb : virtual public x::obj {

public:

	baton batonp;
	x::weakptr<repocontrollermasterptr > master;

	baton_given_cb(const baton &batonArg,
		       const repocontrollermasterptr &masterArg)
;

	~baton_given_cb() noexcept;
};

LOG_FUNC_SCOPE_DECL(repopeerconnectionObj::dispatch::baton_transfer_request, baton_transfer_requestLog);

void repopeerconnectionObj::dispatch(const baton_transfer_request_msg &msg)

{
	LOG_FUNC_SCOPE(baton_transfer_requestLog);

#ifdef DEBUG_BATON_TEST_5_ABORT_BATON_TRANSFER_HOOK
	DEBUG_BATON_TEST_5_ABORT_BATON_TRANSFER_HOOK();
#endif

	if (msg.batonp->replacementnode != peername)
	{
		LOG_FATAL("Baton being transferred to "
			  << msg.batonp->replacementnode
			  << " was somehow received by connection object to "
			  << peername);
		return;
	}

	if (!IS_MASTER(msg.batonp->resigningnode,
		       msg.batonp->resigninguuid))
	{
		LOG_WARNING("Ignoring baton transfer request to "
			    << msg.batonp->replacementnode
			    << " - not currently in master mode");
		return;
	}

	LOG_INFO("Baton being transferred to " << peername);

	auto given_cb=
		x::ref<baton_given_cb>::create(msg.batonp,
					       mastermetaptr->master);

	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<batonisyours> >
		      ::create(msg.batonp->resigningnode,
			       msg.batonp->resigninguuid,
			       msg.batonp->batonuuid));
	(*pings_outstanding)[x::tostring(msg.batonp->batonuuid)]=given_cb;
}

LOG_FUNC_SCOPE_DECL(repopeerconnectionObj::deserialized::batonisyours, deserialized_batonisyoursLog);

void repopeerconnectionObj::deserialized(const batonisyours &msg)

{
	LOG_FUNC_SCOPE(deserialized_batonisyoursLog);

	if (!IS_SLAVE(msg.resigningnode, msg.resigninguuid))
	{
		LOG_ERROR("Rejected baton from "
			  << peername
			  << ", no connection with a slave controller");
		return;
	}

	LOG_INFO("Handoff baton from " << msg.resigningnode
		 << " to me received");

	baton batonp(baton::create(msg.resigningnode,
				   msg.resigninguuid,
				   getthisnodename(),
				   msg.batonuuid));

	x::eventfd eventfd=x::eventfd::create();

	auto commitThread=x::ref<baton_slave_commitlock_thread>
		::create(eventfd, batonp,
			 repopeerconnectionptr(this),
			 getthiscluster());

	batonp->set_commitlock(slavemetaptr->dstrepo->commitlock(eventfd));

	tracker->start(commitThread);

}

void repopeerconnectionObj::dispatch(const batonismine_msg &msg)

{
	LOG_INFO(getthisnodename()
		 << ": connection with " << peername
		 << ": disconnecting from old master, holding baton until transfer is complete");

	getthiscluster()->installformermasterbaton(msg.batonp);
#ifdef DEBUG_BATON_TEST_6_NEWMASTER_FORMERMASTER_DISCONNECT
	DEBUG_BATON_TEST_6_NEWMASTER_FORMERMASTER_DISCONNECT();
#endif

	// The master installed baton_given_cb as a pong callback, trigger it.

	pongm pp;

	pp.uuid=x::tostring(msg.batonp->batonuuid);

	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<pongm> >::create(pp));
}

repopeerconnectionObj::baton_given_cb
::baton_given_cb(const baton &batonArg,
		 const repocontrollermasterptr &masterArg)

	: batonp(batonArg), master(masterArg)
{
}

repopeerconnectionObj::baton_given_cb::~baton_given_cb() noexcept
{
	repocontrollermasterptr masterptr=master.getptr();

	if (!masterptr.null())
		masterptr->masterbaton_handedover(batonp);
}

void repopeerconnectionObj::dispatch(const ping_msg &msg)

{
	pingm pp;

	pp.uuid=x::tostring(x::uuid());

	(*pings_outstanding)[pp.uuid]=msg.mcguffin;

	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<pingm> >::create(pp));
}

void repopeerconnectionObj::deserialized(const pingm &msg)

{
	pongm p;

	p.uuid=msg.uuid;

#ifdef DEBUG_PINGPONG
	DEBUG_PINGPONG();
#endif
	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<pongm> >::create(p));
}

void repopeerconnectionObj::deserialized(const pongm &msg)

{
	pings_outstanding->erase(msg.uuid);
}

void repopeerconnectionObj::dispatch(const master_quorum_announce_msg &msg)

{
	if (thisstatus.master != msg.mastername ||
	    thisstatus.uuid != msg.uuid)
	{
		LOG_WARNING(getthisnodename()
			    << ": connection with " << peername
			    << ": master quorum notification not sent since peer no longer a slave");

		return;
	}

	LOG_INFO(getthisnodename()
		 << ": connection with " << peername
		 << ": notifying of master quorum status update: "
		 << x::tostring(msg.inquorum));

	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<master_quorum> >
		      ::create(msg.mastername,
			       msg.uuid,
			       msg.inquorum));
}

void repopeerconnectionObj::deserialized(const master_quorum &msg)

{
	LOG_INFO(getthisnodename()
		 << ": connection with " << peername
		 << ": master quorum status update: "
		 << msg.mastername << ", uuid=" << x::tostring(msg.masteruuid)
		 << ": " << x::tostring(msg.state));

	if (!IS_SLAVE(msg.mastername, msg.masteruuid))
	{
		LOG_WARNING(getthisnodename()
			    << ": connection with " << peername
			    << ": quorum update not from my master ignored");
		return;
	}

	x::ptr<repocontrollerbaseObj>
		controller=(*currentpeerlink)->controller.getptr();

	if (controller.null())
	{
		LOG_WARNING(getthisnodename()
			    << ": connection with " << peername
			    << ": quorum update failed, controller has terminated");
		return;
	}

	repocontrollerslaveObj *slave=
		dynamic_cast<repocontrollerslaveObj *>(&*controller);

	if (!slave)
	{
		LOG_WARNING(getthisnodename()
			    << ": connection with " << peername
			    << ": quorum update failed, failed to instantiate slave controller object");
		return;
	}

	slave->master_quorum_announce(msg.state);
}

class repopeerconnectionObj::handover_request_cb : virtual public x::obj {

public:
	x::uuid request_uuid;
	x::weakptr<repopeerconnectionptr > conn;

	handover_request_cb(const x::uuid &request_uuidArg,
			    const repopeerconnectionptr &connArg)

		: request_uuid(request_uuidArg), conn(connArg)
	{
	}

	~handover_request_cb() noexcept
	{
		repopeerconnectionptr ptr(conn.getptr());

		if (!ptr.null())
			ptr->handover_request_processed(request_uuid);
	}

	void destroyed()
	{
		// Implemented in the destructor, in case the handover_request
		// cannot be forwarded to the master controller
	}

};

void repopeerconnectionObj::dispatch(const master_handover_request_msg &msg)

{
	x::uuid uuid;

	LOG_INFO(getthisnodename()
		 << ": connection with " << peername
		 << ": sending handover request "
		 << x::tostring(uuid)
		 << " to "
		 << msg.newmastername);

	(*pings_outstanding)[x::tostring(uuid)]=msg.mcguffin;

	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<handover_request> >
		      ::create(msg.newmastername, uuid));
}

void repopeerconnectionObj::deserialized(const handover_request &msg)

{
	auto resp=x::ref<handover_request_cb>
		::create(msg.uuid, repopeerconnectionptr(this));

	if (mastermetaptr->installed())
	{
		LOG_INFO(getthisnodename()
			 << ": connection with " << peername
			 << ": received handover request "
			 << x::tostring(msg.uuid)
			 << " to "
			 << msg.newmastername);

		mastermetaptr->master->handoff_request(msg.newmastername)
			->ondestroy([resp]{resp->destroyed();});
	}
}

void repopeerconnectionObj::dispatch(const handover_request_processed_msg &msg)

{
	LOG_INFO(getthisnodename()
		 << ": connection with " << peername
		 << ": acknowledged handover request "
		 << x::tostring(msg.uuid));

	deserialized(pingm(x::tostring(msg.uuid))); // Send a pong
}

void repopeerconnectionObj::dispatch(const noop_msg &msg)

{
}

repopeerconnectionObj::pingm::pingm()
{
}

repopeerconnectionObj::pingm::~pingm() noexcept
{
}

repopeerconnectionObj::pongm::pongm()
{
}

repopeerconnectionObj::pongm::~pongm() noexcept
{
}

class repopeerconnectionObj::setnewcertObj::pendingObj : virtual public x::obj {

public:

	x::ref<setnewcertObj> req;
	x::ptr<x::obj> mcguffin;

	pendingObj(const x::ref<setnewcertObj> &reqArg,
		   const x::ptr<x::obj> mcguffinArg) : req(reqArg),
						       mcguffin(mcguffinArg)
	{
	}

	~pendingObj() noexcept
	{
	}
};

void repopeerconnectionObj::dispatch(const setnewcert_request_msg &msg)
{
	auto pending = x::ref<setnewcertObj::pendingObj>
		::create(msg.request, msg.mcguffin);

	x::uuid request_uuid;

	auto req = x::ptr<STASHER_NAMESPACE::writtenObj<setnewcert> >
		::create(msg.request->certificate,
		         x::tostring(request_uuid));

	(*pings_outstanding)[req->msg.uuid]=pending;
	writer->write(req);
}

void repopeerconnectionObj::deserialized(const setnewcert &msg)
{
#ifndef REPOPEER_REGRESSION_TEST
	bool success=false;
	std::string resp;

	try {
		success=listener->install(msg.certificate);
		resp=(success
		      ? "Certificate installed on ":
		      "Certificate name mismatch on ")
			+ listener->nodeName();

	} catch (const x::exception &e)
	{
		std::ostringstream o;

		o << e;

		resp=o.str();
	}
	writer->write(x::ptr<STASHER_NAMESPACE::writtenObj<setnewcert_resp> >
		      ::create(success, resp, msg.uuid));
#endif
}

void repopeerconnectionObj::deserialized(const setnewcert_resp &msg)
{
	x::ref<setnewcertObj::pendingObj> req=(*pings_outstanding)[msg.uuid];
	req->req->result=msg.result;
	req->req->success=msg.success;
	pings_outstanding->erase(msg.uuid);
}

void repopeerconnectionObj::dispatch(const halt_request_msg &msg)
{
	if (!IS_MASTER(msg.mastername, msg.masteruuid))
	{
		LOG_INFO(getthisnodename()
			 << ": connection with " << peername
			 << ": ignoring halt request from: "
			 << msg.mastername << ", uuid=" << x::tostring(msg.masteruuid));
		return;
	}

	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<halt> >
		      ::create(msg.mastername, msg.masteruuid));
	mastermetaptr->halt_mcguffin=msg.mcguffin;

	LOG_INFO(getthisnodename()
		 << ": connection with " << peername
		 << ": sent halt request");
}

void repopeerconnectionObj::deserialized(const halt &msg)
{
	LOG_INFO(getthisnodename()
		 << ": connection with " << peername
		 << ": received halt message: "
		 << msg.mastername << ", uuid=" << x::tostring(msg.masteruuid));

	if (!IS_SLAVE(msg.mastername, msg.masteruuid))
	{
		LOG_WARNING(getthisnodename()
			    << ": halt: connection with " << peername
			    << ": not connected with a slave controller, halting anyway");
	}

	adminstopintptr ptr=haltstop->getptr();

	if (!ptr.null() && !socket->null())
	{
		LOG_INFO("Halting this node");
		ptr->stop(*socket);
	}
	else
	{
		LOG_WARNING("Unable to halt this node");
	}
}

std::string repopeerconnectionObj::report(std::ostream &rep)
{
	rep << "Encrypted connection: " << x::tostring(encrypted)
	    << std::endl
	    << "Peer's last reported status: ";

	peerstatusObj::status(this)->toString(rep); rep << std::endl;
	rep << "This node's status: ";
	thisstatus.toString(rep); rep << std::endl;
	rep << "Outstanding ping requests:" << std::endl;

	for (std::map<std::string, x::ptr<x::obj> >::iterator
		     b=pings_outstanding->begin(),
		     e=pings_outstanding->end(); b != e; ++b)
	{
		x::ptr<x::obj> p=b->second;

		rep << "    " << b->first << ": "
		    << (p.null() ? "(null)":p->objname()) << std::endl;
	}

	if (currentpeerlink->null())
		rep << "Not connected with a controller" << std::endl;
	else
		rep << "Connected with controller: "
		    << (*currentpeerlink)->mastername << " ("
		    << x::tostring((*currentpeerlink)->masteruuid) << ")"
		    << std::endl;

	rep << "Disconnection mcguffin " << (currentconnect->null()
					     ? "not ":"") << "installed"
	    << std::endl;

	if (slavemetaptr->installed())
	{
		rep << "Connected to a slave controller:" << std::endl;
		rep << "  Slave controller object installed: "
		    << x::tostring(!slavemetaptr->slave.getptr().null())
		    << std::endl;

		rep << "  Destination repository copy object installed: "
		    << x::tostring(slavemetaptr->dstptr.null())
		    << std::endl;

		rep << "  Source repository copy object installed: "
		    << x::tostring(slavemetaptr->srcmaster.null())
		    << std::endl;

		if (slavemetaptr->slave_baton.null())
			rep << "  Slave baton not received" << std::endl;
		else
			rep << "  Slave baton received: "
			    << (std::string)*slavemetaptr->slave_baton
			    << std::endl;
	}
	else
	{
		rep << "Not connected to a slave controller" << std::endl;
	}

	if (mastermetaptr->installed())
	{
		rep << "Connected to a master controller:" << std::endl;

		rep << "  Slave sync request message received: "
		    << x::tostring(slavesyncrequest_received) << std::endl;
		rep << "  Master controller object present: "
		    << x::tostring(!mastermetaptr->master.null())
		    << std::endl;
		rep << "  Destination copy object present: "
		    << x::tostring(!mastermetaptr->master.null())
		    << std::endl;
		rep << "  Destination repository copy completed: "
		    << x::tostring(mastermetaptr->dstcopy_done)
		    << std::endl;

		rep << "  Baton announcement mcguffin present: "
		    << x::tostring(!mastermetaptr->baton_announce_mcguffin
				   .null())
		    << std::endl;
		rep << "  Halt request sent: "
		    << x::tostring(!mastermetaptr->halt_mcguffin.null())
		    << std::endl;
		rep << "  Commit requests outstanding:"
		    << std::endl;

		for (std::map<x::uuid, x::ptr<x::obj> >::iterator
			     b=mastermetaptr->pending_commits.begin(),
			     e=mastermetaptr->pending_commits.end(); b != e;
		     ++b)
		{
			rep << "    " << x::tostring(b->first) << std::endl;
		}

		// peertrans owned by the controller
	}
	else
	{
		rep << "Not connected to a master controller" << std::endl;
	}

	if (formermasterbaton->null())
	{
		rep << "Former master's baton is not installed" << std::endl;
	}
	else
	{
		rep << "Installed former master's baton: "
		    << (std::string)**formermasterbaton << std::endl;
	}

	if (pendingslavebaton->null())
	{
		rep << "New master's baton is not installed" << std::endl;
	}
	else
	{
		rep << "New former master's baton: "
		    << (std::string)**pendingslavebaton << std::endl;
	}

	if (pendingmasterbaton->null())
	{
		rep << "Pending new master's baton is not installed"
		    << std::endl;
	}
	else
	{
		rep << "Pending new master's baton: "
		    << (std::string)**pendingmasterbaton << std::endl;
	}


	return peername;
}

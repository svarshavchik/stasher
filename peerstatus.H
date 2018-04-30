/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef peerstatus_H
#define peerstatus_H

#include "clusterinfo.H"
#include "nodeclusterstatus.H"
#include "clusterstatusnotifier.H"
#include "objrepocopysrcinterface.H"
#include "time.h"
#include <x/stoppable.H>
#include <shared_mutex>

//! The status of a connection that's registered with clusterinfo

//! This class owns a nodeclusterstatus that represents the status of
//! a peer in the cluster.
//!
//! \par Registration with a clusterinfo [INSTALL]
//!
//! The subclass invokes install() to register itself with a clusterinfo
//! under the specified name. install() invokes clusterinfoObj::installpeer().
//!
//! \par Updating the status [UPDATE]
//!
//! The subclass invokes peerstatusupdate() upon receiving the status from a
//! peer. peerstatusupdate() records the status.
//! If install() was previously invoked, peerstatusupdate() invokes
//! clusterinfoObj::peerstatusupdated(). If the peer is now reporting a new
//! master, peerstatusupdate() invokes clusterinfoObj::peernewmaster();
//!
//! \par Destructor action [ONDESTROY]
//!
//! If install() succeeds, it may not be called again.
//! It installs a destructor callback on this
//! object that invokes clusterinfoObj::recalculate() upon this object's
//! destruction.

class peerstatusObj : public clusterstatusnotifierObj {

	//! A lock on the status object

	std::shared_mutex lock;

	//! Registered cluster
	clusterinfoptr cluster;

	//! Load the initial cluster status

	virtual void initialstatus(const nodeclusterstatus &newStatus)
		override=0;

	//! Peer's status, as set by the subclass that calls peerstatusupdate()

	//! \note
	//! The subclass cannot access this directly, must use status,
	//! like everyone else.

	nodeclusterstatus curpeerstatus;


protected:
	//! Return this node's name, for debugging purposes

	const std::string &getthisnodename() const
	{
		if (cluster.null())
			return curpeerstatus.master; // Regression test

		return cluster->getthisnodename();
	}

	//! Return this connection object's cluster

	clusterinfo getthiscluster() const
	{
		return cluster;
	}

	//! Register this object with the cluster.
	std::pair<bool, x::ptr<peerstatusObj> >
	install(const clusterinfo &clusterArg);

	//! Update this object's status.
	void peerstatusupdate(const nodeclusterstatus &newstatus);


public:
	//! Timestamp when the connection was initiated

	time_t timestamp;

	//! Connection uuid
	x::uuid connuuid;

	friend class clusterinfoObj;

	//! Peer's name
	const std::string peername;

	//! Constructor
	peerstatusObj(const std::string &peernameArg);
	~peerstatusObj();

	class status;

	friend class status;

	class adapterObj;
};

//! A reference to a peerstatusObj

typedef x::ptr<peerstatusObj> peerstatus;

//! This peer's status

//! Instantiate the status object to retrieve the peer's latest status.
//! This object acts like a pointer to a nodeclusterstatus,
//! and defines the -> and * operators that access the object.
//! This is a thread-safe operation with respect to
//! peerstatusObj::statusupdate(). [ACCESS]

class peerstatusObj::status {

	//! My connection node
	peerstatus node;

	//! A read lock on the status
	std::shared_lock<std::shared_mutex> lock;

public:
	//! Constructor
	status(const peerstatus &nodeArg)
		: node(nodeArg), lock(node->lock)
	{
	}

	//! Constructor
	status(peerstatusObj *thisptr)
		: node(peerstatus(thisptr)), lock(node->lock)
	{
	}

	//! Destructor
	~status()
	{
	}

	//! Dereference operator

	const nodeclusterstatus *operator->()
	{
		return &node->curpeerstatus;
	}

	//! Dereference operator

	const nodeclusterstatus &operator*()
	{
		return node->curpeerstatus;
	}
};

//! An adapter for testing purposes.

//! Subclass peerstatusObj and provide no-ops for all virtual methods.
//!

class peerstatusObj::adapterObj : public peerstatusObj {

public:
	//! Constructor
	adapterObj(const std::string &peernameArg);

	//! Destructor
	~adapterObj();

	void statusupdated(const nodeclusterstatus &newStatus) override;


	void initialstatus(const nodeclusterstatus &newStatus) override;

};

#endif

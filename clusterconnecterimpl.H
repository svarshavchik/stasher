/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef clusterconnecterimpl_H
#define clusterconnecterimpl_H

#include "clusterconnecter.H"
#include "spacemonitor.H"

#include "stasher/namespace.H"

#include "stoppablethreadtrackerfwd.H"

class trandistributorObj;
class clustertlsconnectshutdownObj;
class tobjrepoObj;

//! Default implementation of clusterconnecterObj::connected().

//! A subclass of clusterconnecterObj that implements connected() by
//! instantiating a repopeerconnectionObj. If the peer connection uses TLS,
//! when the connection thread terminates, a clustertlsconnectshutdownObj
//! thread gets started that implements an orderly TLS session shutdown.

class clusterconnecterimplObj : public clusterconnecterObj {

	//! Connection's cluster
	clusterinfo cluster;

	//! Tracker that starts the shutdown thread
	STASHER_NAMESPACE::stoppableThreadTracker tracker;

	//! The shutdown thread
	x::ptr<clustertlsconnectshutdownObj> shutdown;

	//! Distributor object for the connection thread
	x::ptr<trandistributorObj> distributor;

	//! Repository being distributed
	tobjrepo repo;

	//! Space monitoring object
	spacemonitor df;

public:
	//! Constructor
	clusterconnecterimplObj(//! Peer's name
				const std::string &name,
				//! Connection's cluster
				const clusterinfo &clusterArg,
				//! Tracker that starts the shutdown thread
				const STASHER_NAMESPACE::stoppableThreadTracker
				&trackerArg,
				//! The shutdown thread
				const x::ptr<clustertlsconnectshutdownObj>
				&shutdownArg,
				//! Distributor object for the connection thread
				const x::ptr<trandistributorObj>&distributorArg,
				//! Repository being distributed
				const tobjrepo &repoArg,
				//! Space monitoring object
				const spacemonitor &dfArg);


	//! Destructor
	~clusterconnecterimplObj();

	//! Start a repopeerconnectionobj

	void connected(const std::string &peername,
		       const x::fd &socket,
		       const x::gnutls::sessionptr &session,
		       const x::fd::base::inputiter &inputiter,
		       const nodeclusterstatus &peerstatus,
		       time_t connect_timestamp,
		       const x::uuid &connuuid,
		       const clusterlistener &listener);

};

#endif

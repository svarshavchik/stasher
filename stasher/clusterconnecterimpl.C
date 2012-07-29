/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "clusterconnecterimpl.H"
#include "clusterinfo.H"
#include "stoppablethreadtracker.H"
#include "trandistributor.H"
#include "clustertlsconnectshutdown.H"
#include "repopeerconnection.H"
#include "clusterlistener.H"
#include "tobjrepo.H"

clusterconnecterimplObj
::clusterconnecterimplObj(const std::string &name,
			  const clusterinfo &clusterArg,
			  const STASHER_NAMESPACE::stoppableThreadTracker &trackerArg,
			  const x::ptr<clustertlsconnectshutdownObj>
			  &shutdownArg,
			  const x::ptr<trandistributorObj> &distributorArg,
			  const tobjrepo &repoArg,
			  const spacemonitor &dfArg)

	: clusterconnecterObj(name), cluster(clusterArg), tracker(trackerArg),
	  shutdown(shutdownArg),
	  distributor(distributorArg), repo(repoArg), df(dfArg)
{
}

clusterconnecterimplObj::~clusterconnecterimplObj() noexcept
{
}

void clusterconnecterimplObj::connected(const std::string &peername,
					const x::fd &socket,
					const x::gnutls::sessionptr &session,
					const x::fd::base::inputiter &inputiter,
					const nodeclusterstatus &peerstatus,
					time_t connectTimestamp,
					const x::uuid &connuuid,
					const clusterlistener &listener)
{
	socket->nonblock(true); // Needed by some regression tests

	x::ptr<x::obj> mcguffin(x::ptr<x::obj>::create());

	if (!session.null())
		clustertlsconnectshutdownObj
			::create(mcguffin, tracker, socket, session, shutdown);

	auto connection=repopeerconnection::create(peername, df);

	connection->timestamp=connectTimestamp;
	connection->connuuid=connuuid;

	tracker->start(connection,
		       (session.null() ? x::fdbase(socket):x::fdbase(session)),
		       inputiter,
		       tracker,
		       mcguffin,
		       session.null() ? false:true,
		       distributor,
		       listener,
		       peerstatus,
		       cluster,
		       repo);
}

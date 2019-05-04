/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "peerstatus.H"

#include <x/logger.H>

peerstatusObj::peerstatusObj(const std::string &peernameArg)
	: timestamp(0), peername(peernameArg)
{
}

peerstatusObj::~peerstatusObj()
{
}

LOG_FUNC_SCOPE_DECL(peerstatusObj::install, peerinstallLog);

std::pair<bool, peerstatus>
peerstatusObj::install(const clusterinfo &clusterArg)
{
	LOG_FUNC_SCOPE(peerinstallLog);

	if (!cluster.null())
		throw EXCEPTION("Internal error: peerstatusObj::install called more than once, or called with a null object");

	std::pair<bool, peerstatus> ret=
		clusterArg->installpeer(peername, peerstatus(this));

	LOG_DEBUG(clusterArg->getthisnodename() << ": status of "
		  << peername << " installation attempt: "
		  << x::to_string(ret.first));

	if (ret.first)
		cluster=clusterArg;

	return ret;
}

LOG_FUNC_SCOPE_DECL(peerstatusObj::peerstatusupdate, peerstatusupdateLog);

void peerstatusObj::peerstatusupdate(const nodeclusterstatus &newstatus)

{
	LOG_FUNC_SCOPE(peerstatusupdateLog);

	bool newmaster;

	{
		std::unique_lock<std::shared_mutex> wlock{lock};

		newmaster= curpeerstatus.master != newstatus.master;

		curpeerstatus=newstatus;
	}

	LOG_DEBUG( ({
				std::ostringstream o;

				if (cluster.null())
					o << "Ignoring status update from "
					  << peername << ": not connected yet";
				else
					o << getthisnodename() << ": status of "
					  << peername << " updated";

				o.str();
			}));

	if (!cluster.null())
	{
		LOG_DEBUG(getthisnodename() << ": notifying cluster of "
			  << peername << "'s status");
		cluster->peerstatusupdated(peerstatus(this), newstatus);

		if (newmaster)
		{
			LOG_DEBUG(getthisnodename() << ": new master: "
				  << peername << "'s master is now "
				  << newstatus.master);

			cluster->peernewmaster(peerstatus(this), newstatus);
		}
	}
}

// ------

peerstatusObj::adapterObj::adapterObj(const std::string &peernameArg)
 : peerstatusObj(peernameArg)
{
}

peerstatusObj::adapterObj::~adapterObj()
{
}

void peerstatusObj::adapterObj::statusupdated(const nodeclusterstatus
					      &newStatus)

{
}

void peerstatusObj::adapterObj::initialstatus(const nodeclusterstatus
					      &newStatus)

{
}

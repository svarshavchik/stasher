/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "mastersyncinfo.H"
#include "repocontrollermasterslaveconnection.H"
#include "trandistuuid.H"

mastersyncinfoObj
::mastersyncinfoObj(const std::string &masternameArg,
		    const x::uuid &masteruuidArg,
		    const repocontrollermasterptr &controllerArg,
		    const x::ptr<repocontrollermasterObj
				 ::slaveConnectionObj> &connectionArg,
		    const trandistuuid &received_uuidsArg)
	: mastername(masternameArg),
	  masteruuid(masteruuidArg),
	  controller(controllerArg),
	  connection(connectionArg),
	  received_uuids(received_uuidsArg)
{
}

mastersyncinfoObj::~mastersyncinfoObj()
{
}

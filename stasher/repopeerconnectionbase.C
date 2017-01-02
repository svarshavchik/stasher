/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "repopeerconnectionbase.H"
#include "repocontrollerbase.H"
#include <x/weakptr.H>

repopeerconnectionbaseObj::peerlinkObj
::peerlinkObj(const x::ptr<repocontrollerbaseObj> &controllerArg,
	      const x::ptr<x::obj> &controller_mcguffinArg,
	      const std::string &masternameArg,
	      const x::uuid &masteruuidArg)
 : controller(controllerArg),
			      controller_mcguffin(controller_mcguffinArg),
			      mastername(masternameArg),
			      masteruuid(masteruuidArg)
{
}

repopeerconnectionbaseObj::peerlinkObj::~peerlinkObj()
{
}

repopeerconnectionbaseObj::repopeerconnectionbaseObj()
{
}

repopeerconnectionbaseObj::~repopeerconnectionbaseObj()
{
}

repopeerconnectionbaseObj::peerdisconnect_msgObj
::peerdisconnect_msgObj(const x::ptr<repopeerconnectionbaseObj> &objArg)
 : connection_obj(objArg)
{
}

repopeerconnectionbaseObj::peerdisconnect_msgObj
::~peerdisconnect_msgObj()
{
}

void repopeerconnectionbaseObj::peerdisconnect_msgObj
::destroyed(const x::ptr<repocontrollerbaseObj> &dummy)

{

	x::ptr<repopeerconnectionbaseObj> obj(connection_obj.getptr());

	if (!obj.null())
		obj->disconnect_peer();
}

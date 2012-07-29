/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "batonhandoverthread.H"
#include "clusterinfo.H"

#include <x/destroycallbackobj.H>

LOG_CLASS_INIT(batonhandoverthreadObj);

#include "batonhandoverthread.msgs.def.H"

batonhandoverthreadObj::batonhandoverthreadObj()
{
}

batonhandoverthreadObj::~batonhandoverthreadObj() noexcept
{
}

std::string batonhandoverthreadObj::getName() const
{
	return "batonhandover";
}

class batonhandoverthreadObj::baton_destroyed_callback
	: public x::destroyCallbackObj {
public:
	x::weakptr<x::ptr<batonhandoverthreadObj> > thr;

	baton_destroyed_callback(const x::ptr<batonhandoverthreadObj> &thrArg)
		: thr(thrArg)
	{
	}

	~baton_destroyed_callback() noexcept
	{
	}

	void destroyed() noexcept
	{
		x::ptr<batonhandoverthreadObj> ptr=thr.getptr();

		if (!ptr.null())
			ptr->baton_destroyed();
	}
};

class batonhandoverthreadObj::quorum_status_callback :
	public STASHER_NAMESPACE::repoclusterquorumcallbackbaseObj {

public:

	x::weakptr<x::ptr<batonhandoverthreadObj> > thr;

	quorum_status_callback(const x::ptr<batonhandoverthreadObj> &thrArg)
 : thr(thrArg)
	{
	}

	~quorum_status_callback() noexcept
	{
	}
	
	void quorum(const STASHER_NAMESPACE::quorumstate &inquorum)

	{
		x::ptr<batonhandoverthreadObj> ptr=thr.getptr();

		if (!ptr.null())
			ptr->quorum(inquorum.full);
	}
};

void batonhandoverthreadObj::run(const std::string &newmasterref,
				 const boolref &completed_statusref,
				 x::weakptr<x::ptr<x::obj> > &baton_handedover,
				 const x::ptr<repoclusterquorumObj>
				 &clusterquorum,
				 const clusterinfo &clusterref)
{
	baton_destroyed_flag=false;
	quorum_flag=false;

	auto quorum_status_callbackref=
		x::ref<quorum_status_callback>::create
		(x::ref<batonhandoverthreadObj>(this));

	{
		auto ptr=baton_handedover.getptr();

		if (ptr.null())
			ptr=x::ptr<x::obj>::create();
		// use a dummy one, for immediate effect.

		ptr->addOnDestroy(x::ref<baton_destroyed_callback>
				  ::create
				  (x::ptr<batonhandoverthreadObj>(this)));
	}

	clusterquorum->install(quorum_status_callbackref);

	LOG_INFO("Started handover to " << newmasterref);

	newmaster= &newmasterref;
	completed_status=&completed_statusref;
	cluster= &clusterref;

	while (1)
		msgqueue->pop()->dispatch();
}

void batonhandoverthreadObj::dispatch(const baton_destroyed_msg &msg)

{
	LOG_DEBUG("Baton has been destroyed");
	baton_destroyed_flag=true;
	check_transfer_completed();
}

void batonhandoverthreadObj::dispatch(const quorum_msg &msg)

{
	LOG_DEBUG("Quorum status: " << x::tostring(msg.flag));
	quorum_flag=msg.flag;
	check_transfer_completed();
}

void batonhandoverthreadObj::check_transfer_completed()
{
	if (!baton_destroyed_flag || !quorum_flag)
		return;

	(*completed_status)->flag=
		clusterinfoObj::status(*cluster)->master == *newmaster;

	LOG_DEBUG("Transfer status: "
		  << x::tostring((*completed_status)->flag));

	stop();
}


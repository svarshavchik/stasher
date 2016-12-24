/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "batonhandoverthread.H"
#include "clusterinfo.H"

LOG_CLASS_INIT(batonhandoverthreadObj);

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
	: virtual public x::obj {
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

void batonhandoverthreadObj::run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
				 const std::string &newmasterref,
				 const boolref &completed_statusref,
				 x::weakptr<x::ptr<x::obj> > &baton_handedover,
				 const x::ptr<repoclusterquorumObj>
				 &clusterquorum,
				 const clusterinfo &clusterref)
{
	msgqueue_auto msgqueue(this);
	threadmsgdispatcher_mcguffin=nullptr;

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

		auto callback=x::ref<baton_destroyed_callback>::create
			       (x::ptr<batonhandoverthreadObj>(this));
		ptr->ondestroy([callback] {callback->destroyed();});
	}

	clusterquorum->install(quorum_status_callbackref);

	LOG_INFO("Started handover to " << newmasterref);

	newmaster= &newmasterref;
	completed_status=&completed_statusref;
	cluster= &clusterref;

	while (1)
		msgqueue.event();
}

void batonhandoverthreadObj::dispatch_baton_destroyed()
{
	LOG_DEBUG("Baton has been destroyed");
	baton_destroyed_flag=true;
	check_transfer_completed();
}

void batonhandoverthreadObj::dispatch_quorum(bool flag)
{
	LOG_DEBUG("Quorum status: " << x::tostring(flag));
	quorum_flag=flag;
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

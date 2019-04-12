/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "repocontrollerbase.H"
#include "tobjrepo.H"
#include <x/destroy_callback.H>

LOG_CLASS_INIT(repocontrollerbaseObj);

repocontroller_start_infoObj
::repocontroller_start_infoObj(const repocontrollerbase &new_controller)
	: new_controller(new_controller),
	  new_controller_queue(x::threadmsgdispatcherObj::msgqueue_obj
			       ::create(new_controller))
{
}

repocontroller_start_infoObj::~repocontroller_start_infoObj()=default;

void repocontroller_start_infoObj::start(const x::ref<x::obj> &mcguffin)
{
	new_controller->start_controller(new_controller_queue, mcguffin);
}

///////////////////////////////////////////////////////////////////////////////

repocontrollerbaseObj
::repocontrollerbaseObj(const std::string &masternameArg,
			const x::uuid &masteruuidArg,
			const tobjrepo &repoArg,
			const repoclusterquorum &callback_listArg)
	: callback_list(callback_listArg),
	  quorumstatus_mcguffin(x::ptr<x::obj>::create()),
	  mastername(masternameArg),
	  masteruuid(masteruuidArg),
	  repo(repoArg),

	  next_controller_mcguffin(repoArg) // Need to stick something in here
{
	repoclusterquorum::base::updatelock ulock(*callback_list);

	STASHER_NAMESPACE::quorumstate oldstatus, newstatus;

	{
		repoclusterquorum::base::writelock wlock(*callback_list);

		wlock->mcguffins->push_back(quorumstatus_mcguffin);
		oldstatus=wlock->value;
		wlock->value=newstatus;
	}

	if (oldstatus != newstatus) // Not in quorum any more
		ulock.notify(newstatus);
}

repocontrollerbaseObj::~repocontrollerbaseObj()
{
	try {
		started(); // Clear the mcguffin, just in case
		if (!next_controller.null())
		{
			LOG_WARNING("Starting successor controller");
			next_controller->start(next_controller_mcguffin);
		}
	} catch (const x::exception &e)
	{
		LOG_FATAL(e);
	}
}

void repocontrollerbaseObj::started()
{
	quorumstatus_mcguffin=nullptr;
}

void repocontrollerbaseObj::handoff_next(const
					 repocontroller_start_info &next,
					 const x::ref<x::obj> &mcguffin)

{
	LOG_WARNING("Handing off repository controller");
	next_controller=next;
	next_controller_mcguffin=mcguffin;
}

STASHER_NAMESPACE::quorumstate
repocontrollerbaseObj::quorum(const STASHER_NAMESPACE::quorumstate &inquorum)

{
	LOG_INFO("Announcing quorum status: " << x::tostring(inquorum));

	repoclusterquorum::base::updatelock lock(*callback_list);

	STASHER_NAMESPACE::quorumstate flag=
		({
			repoclusterquorum::base::writelock
				wlock(*callback_list);

			STASHER_NAMESPACE::quorumstate newval =
				wlock->reported_status(inquorum);

			if (newval == wlock->value)
				return newval;

			wlock->value=newval;

			newval;
		});

	lock.notify(flag);
	return flag;
}

STASHER_NAMESPACE::quorumstate repocontrollerbaseObj::inquorum()

{
	return *repoclusterquorum::base::readlock(*callback_list);
}

STASHER_NAMESPACE::quorumstate
repocontrollerbaseObj::debug_inquorum()
{
	STASHER_NAMESPACE::quorumstateref
		status(STASHER_NAMESPACE::quorumstateref::create());

	boolref processed(boolref::create());
	x::ptr<x::obj> mcguffin(x::ptr<x::obj>::create());

	get_quorum(status, processed, mcguffin);

	x::destroy_callback cb=x::destroy_callback::create();

	mcguffin->ondestroy([cb]{cb->destroyed();});
	mcguffin=nullptr;

	cb->wait();

	// Expect to succeed!

	return *status;
}

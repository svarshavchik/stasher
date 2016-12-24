/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "repocontrollerbasehandoff.H"

repocontrollerbasehandoffObj
::repocontrollerbasehandoffObj(const std::string &masternameArg,
			       const x::uuid &masteruuidArg,
			       const tobjrepo &repoArg,
			       const repoclusterquorum &callback_listArg,
			       const STASHER_NAMESPACE::stoppableThreadTracker &trackerArg)

	: repocontrollerbaseObj(masternameArg,
				masteruuidArg,
				repoArg,
				callback_listArg),
	  tracker(trackerArg)
{
}

repocontrollerbasehandoffObj::~repocontrollerbasehandoffObj() noexcept
{
}

void repocontrollerbasehandoffObj
::dispatch_do_handoff(const repocontroller_start_info &next)
{
	handoff_next(next, *mcguffin);
	stop();
}

void repocontrollerbasehandoffObj::handoff(const repocontroller_start_info &next)
{
	do_handoff(next);
}

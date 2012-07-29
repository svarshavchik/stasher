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

void repocontrollerbasehandoffObj::dispatch(const handoff_msg &msg)

{
	handoff_next(msg.next, *mcguffin);
	stop();
}

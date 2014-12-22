/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "repoclusterquorum.H"

quorumstatus_impl::quorumstatus_impl()
	: mcguffins(x::weaklist<x::obj>::create())
{
}

quorumstatus_impl::~quorumstatus_impl() noexcept
{
}

STASHER_NAMESPACE::quorumstate
quorumstatus_impl::reported_status(const STASHER_NAMESPACE::quorumstate
				   &statusArg)
{
	for (auto mcguffin: *mcguffins)
		if (!mcguffin.getptr().null())
			return STASHER_NAMESPACE::quorumstate();
	return statusArg;
}

repoclusterquorumObj::repoclusterquorumObj()
{
}

repoclusterquorumObj::~repoclusterquorumObj() noexcept
{
}

void repoclusterquorumObj::install(const
				   x::ref<STASHER_NAMESPACE
					  ::repoclusterquorumcallbackbaseObj>
				   &notifier)
{
	handlerlock handler_lock(*this);

	STASHER_NAMESPACE::quorumstate value=*readlock(*this);

	handler_lock.attach_back(notifier,
				 []
				 (const x::ref<STASHER_NAMESPACE
				  ::repoclusterquorumcallbackbaseObj>
				  &notifier,
				  const STASHER_NAMESPACE::quorumstate &value)
				 {
					 notifier->quorum(value);
				 }, value);
}

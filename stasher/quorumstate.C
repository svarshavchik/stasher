/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "stasher/quorumstate.H"

STASHER_NAMESPACE_START

quorumstate::quorumstate()
	: full(false), majority(false)
{
}

quorumstate::quorumstate(bool fullArg,
			 bool majorityArg)
	: full(fullArg), majority(majorityArg)
{
}

quorumstate::~quorumstate()
{
}


STASHER_NAMESPACE_END

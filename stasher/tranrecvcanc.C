/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tranrecvcanc.H"
#include "trandistuuid.H"

tranrecvcanc::tranrecvcanc()
	: received(trandistuuid::create()), cancelled(tranuuid::create())
{
}

tranrecvcanc::tranrecvcanc(const std::string &masternameArg,
			   const x::uuid &masteruuidArg)
 : mastername(masternameArg),
			      masteruuid(masteruuidArg),
			      received(trandistuuid::create()),
			      cancelled(tranuuid::create())
{
}

tranrecvcanc::~tranrecvcanc() noexcept
{
}


/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "usergetuuidsreply.H"

STASHER_NAMESPACE_START

usergetuuidsreply::usergetuuidsreply()
	: uuids(x::ptr<objvecObj>::create()), succeeded(true),
	  errmsg("Success")
{
}

usergetuuidsreply::usergetuuidsreply(const x::uuid &requuidArg)

	: requuid(requuidArg), uuids(x::ptr<objvecObj>::create()),
	  succeeded(true), errmsg("Success")
{
}

usergetuuidsreply::~usergetuuidsreply() noexcept
{
}


STASHER_NAMESPACE_END

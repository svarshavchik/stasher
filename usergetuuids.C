/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "usergetuuids.H"

STASHER_NAMESPACE_START

usergetuuids::usergetuuids()
	: limit(0), reqinfo(x::ptr<reqinfoObj>::create())
{
}

usergetuuids::usergetuuids(size_t limitArg)
	: limit(limitArg), reqinfo(x::ptr<reqinfoObj>::create())
{
}

usergetuuids::~usergetuuids()
{
}

usergetuuids::reqinfoObj::reqinfoObj()
	: openobjects(false), admin(false)
{
}

usergetuuids::reqinfoObj::~reqinfoObj()
{
}

STASHER_NAMESPACE_END

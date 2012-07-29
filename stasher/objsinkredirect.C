/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "objsinkredirect.H"

STASHER_NAMESPACE_START

objsinkredirectObj::objsinkredirectObj(const objsink &sinkArg)
 : sink(sinkArg)
{
}

objsinkredirectObj::~objsinkredirectObj() noexcept
{
}

void objsinkredirectObj::write(const x::ref<writtenobjbaseObj> &object)

{
	sink->write(object);
}

STASHER_NAMESPACE_END

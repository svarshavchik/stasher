/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "userchanged.H"

STASHER_NAMESPACE_START

userchanged::userchanged()
{
}

userchanged::~userchanged()
{
}

userchanged::userchanged(const std::string &objnameArg,
			 const std::string &suffixArg)
	: objname(objnameArg), suffix(suffixArg)
{
}

STASHER_NAMESPACE_END

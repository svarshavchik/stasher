/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "stasher/versionscollected.H"

STASHER_NAMESPACE_START

versionscollected versionscollectedBase::create_from()
{
	return versionscollected::create();
}

versionscollectedObj::versionscollectedObj()
{
}

versionscollectedObj::~versionscollectedObj() noexcept
{
}

bool versionscollectedObj::if_all_unchanged(std::list<x::ref<x::obj> >
					    &versionsListRef)
{
	for (auto &weakptr:versions)
	{
		auto ptr=weakptr.getptr();

		if (ptr.null())
			return false;

		versionsListRef.emplace_back(ptr);
	}
	return true;
}

STASHER_NAMESPACE_END

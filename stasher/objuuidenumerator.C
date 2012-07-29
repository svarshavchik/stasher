/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "objuuidenumerator.H"
#include "objuuidlist.H"
#include "objsink.H"
#include "objwriter.H"
#include <x/destroycallbackobj.H>

objuuidenumeratorObj::objuuidenumeratorObj(const tobjrepo &repoArg)
	: repo(repoArg),
	  b(repo->obj_begin()),
	  e(repo->obj_end())
{
}

objuuidenumeratorObj::~objuuidenumeratorObj() noexcept
{
}

objuuidlist objuuidenumeratorObj::getobjuuidlist()
{
	return objuuidlist::create();
}

objuuidlist objuuidenumeratorObj::next()
{
#if 0
	if (repo.null())
		return objuuidlist();
#endif

	std::set<std::string> objectnames;

	for (size_t n=objuuidlistObj::default_chunksize.getValue(); n > 0; --n)
	{
		if (b == e)
			break;

		objectnames.insert(*b);
		++b;
	}

	if (objectnames.empty())
		return objuuidlist();

	return nextbatch(objectnames);
}

objuuidlist objuuidenumeratorObj::nextbatch(const std::set<std::string>
					    &objectnames)
{
	tobjrepoObj::values_t values;
	std::set<std::string> dummy;

	repo->values(objectnames, false, values, dummy);

	objuuidlist uuid(getobjuuidlist());

	for (tobjrepoObj::values_t::const_iterator
		     b(values.begin()), e(values.end()); b != e; ++b)
		uuid->objuuids.insert(std::make_pair(b->first,
						     b->second.first));

	return uuid;
}

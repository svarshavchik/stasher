/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "newtran.H"
#include "tobjrepo.H"

LOG_CLASS_INIT(newtranObj);

newtranObj::newtranObj(const tobjrepo &repoArg,
		       const x::uuid &uuidArg)
	: uuid(uuidArg), repo(repoArg), finalized(false)
{
}

newtranObj::~newtranObj() noexcept
{
	if (!finalized)
	{
		try {
			for (size_t i=0; i<meta.objects.size(); ++i)
				repo->tmp_remove(uuid, i);
		} catch (const x::exception &e)
		{
			LOG_FATAL(e);
		}
	}
}

x::fd newtranObj::newobj(const std::string &objname)
{
	return repo->tmp_create(uuid, meta.newobj(objname));
}

x::fd newtranObj::updobj(const std::string &objname,
			 const x::uuid &sv)
{
	return repo->tmp_create(uuid, meta.updobj(objname, sv));
}

void  newtranObj::delobj(const std::string &objname,
			 const x::uuid &sv)
{
	meta.delobj(objname, sv);
}


x::uuid newtranObj::finalize()
{
	repo->finalize(newtran(this));

	finalized=true;

	return uuid;
}


void newtranObj::objsizes(objsizes_t &objsizeret) const
{
	for (size_t n=meta.objects.size(); n > 0; )
	{
		--n;
		if (meta.objects[n].has_new_value())
			objsizeret.insert
				(std::make_pair
				 (meta.objects[n].name,
				  repo->tmp_reopen(uuid, n)->stat()->st_size));
	}
}

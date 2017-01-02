/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "trancommit.H"

trancommitObj::trancommitObj(const x::uuid &uuidArg,
			     const tobjrepo &repoArg)
	: tranObj(uuidArg), repo(repoArg)
{
}

trancommitObj::~trancommitObj()
{
}

bool trancommitObj::verify() const
{
	for (std::vector<objinfo>::const_iterator b=objects.begin(),
		     e(objects.end()); b != e; ++b)
	{
		x::fileattr attr(repo->obj_get(b->name));

		std::string uuid_str;

		try {
			uuid_str=attr->getattr(repo->xattrserial);
		} catch (...) {

			// Object does not exist

			if (b->has_existing_serial())
				return false;
			continue;
		}

		// Object exists

		if (!b->has_existing_serial())
			return false;

		if (x::uuid(uuid_str) != b->serial)
			return false;
	}

	return true;
}

void trancommitObj::commit()
{
	if (!ready())
		throw EXCEPTION("Internal error: transaction lock is not acquired");
	repo->commit(uuid, *this, lock);
	repo=tobjrepoptr();
	lock=tobjrepoObj::lockentryptr_t();
}


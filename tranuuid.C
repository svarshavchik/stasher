/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tranuuid.H"

tranuuidObj::tranuuidObj()
{
}

tranuuidObj::~tranuuidObj()
{
}

void tranuuidObj::received(const tranuuid &recvuuids)
{
	uuids.insert(recvuuids->uuids.begin(), recvuuids->uuids.end());
}

void tranuuidObj::cancelled(const tranuuid &canuuids)
{
	for (std::set<x::uuid>::const_iterator
		     b(canuuids->uuids.begin()), e(canuuids->uuids.end());
	     b != e; ++b)
		uuids.erase(*b);
}

void tranuuidObj::to_string(std::ostream &o)
{
	for (std::set<x::uuid>::iterator
		     b=uuids.begin(), e=uuids.end(); b != e; ++b)
	{
		o << "        " << x::tostring(*b) << std::endl;
	}
}


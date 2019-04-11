/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "trandistuuid.H"

trandistuuidObj::trandistuuidObj()
{
}

trandistuuidObj::~trandistuuidObj()
{
}

void trandistuuidObj::received(const trandistuuid &recvuuids)

{
	uuids.insert(recvuuids->uuids.begin(), recvuuids->uuids.end());
}

void trandistuuidObj::cancelled(const tranuuid &canuuids)
{
	for (std::set<x::uuid>::const_iterator
		     b(canuuids->uuids.begin()), e(canuuids->uuids.end());
	     b != e; ++b)
		uuids.erase(*b);
}

void trandistuuidObj::to_string(std::ostream &o)
{
	for (std::map<x::uuid, dist_received_status_t>::iterator
		     b=uuids.begin(), e=uuids.end(); b != e; ++b)
	{
		o << "        " << x::tostring(b->first)
		  << ": status " << b->second.status
		  << ", source " << b->second.sourcenode << std::endl;
	}
}


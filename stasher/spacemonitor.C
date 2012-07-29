/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "spacemonitor.H"

x::property::value<long>
spacemonitorObj::reserved_refresh(L"reserved::refresh", 100);

spacemonitorObj::spacemonitorObj(const x::df &freespacerefArg)
 : freespaceref(freespacerefArg)
{
	reset_refresh_counter();
}

spacemonitorObj::~spacemonitorObj() noexcept
{
}

void spacemonitorObj::reset_refresh_counter()
{
	std::lock_guard<std::mutex> lock(objmutex);
	long n=reserved_refresh.getValue();

	if (n < 1)
		n=1;

	if (refresh_counter.refget() > 0)
		return; // Another thread beat me to it

	while (refresh_counter.refadd(n) <= 0)
		;

	freespaceref->refresh();
}

spacemonitorObj::reservation
spacemonitorObj::reservespace_alloc(long alloc, long inodes)

{
	if (refresh_counter.refadd(-1) == 0)
		reset_refresh_counter();

	return freespaceref->reserve(alloc, inodes);
}

bool spacemonitorObj::outofspace()
{
	if (freespaceref->inodeFree() == 0 ||
	    freespaceref->allocFree() == 0)
	{
		freespaceref->refresh();

		if (freespaceref->inodeFree() == 0 ||
		    freespaceref->allocFree() == 0)
			return true;
	}
	return false;
}

uint64_t spacemonitorObj::freespacemb()
{
	uint64_t n=freespaceref->allocFree();
	uint64_t frag=freespaceref->allocSize();

	if (frag < 1024L * 1024L)
		n /= 1024L * 1024L / frag;
	else
		n = n / frag * (1024L * 1024L);

	return n;
}

uint64_t spacemonitorObj::freeinodes()
{
	return freespaceref->inodeFree();
}

long spacemonitorObj::calculate_inode(const std::string &objectname)

{
	long n=1;

	for (std::string::const_iterator b=objectname.begin(),
		     e=objectname.end(); b != e; ++b)
		if (*b == '/')
			++n;
	return n;
}


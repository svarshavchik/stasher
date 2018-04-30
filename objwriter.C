/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "objwriter.H"

#include <algorithm>

STASHER_NAMESPACE_START

x::property::value<size_t>
objwriterObj::default_bufsize("objrepo::writebufsize", 8192);

objwriterObj::objwriterObj(size_t bufsize)
 : buf_ptr(0), ser_iter(*this)
{
	buffer.resize(bufsize);
}

objwriterObj::~objwriterObj()
{
}

void objwriterObj::pubflush()
{
	if (!buffered())
		return;

	size_t n=flush(&buffer[0], buf_ptr);

	if (n < buf_ptr)
		std::copy(&buffer[n], &buffer[0]+buf_ptr, &buffer[0]);

	buf_ptr -= n;
}

STASHER_NAMESPACE_END

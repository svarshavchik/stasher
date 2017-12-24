/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "stasher/userhelo.H"

STASHER_NAMESPACE_START

userhelo::userhelo()
{
}

userhelo::userhelo(const userinit &limitsArg)
	: limits(limitsArg)
{
}

userhelo::~userhelo()
{
}

userinit::userinit()
	: maxgets(0),
	  maxobjects(0),
	  maxobjectsize(0),
	  chunksize(0),
	  maxsubs(0)
{
}

userinit::userinit(size_t maxgetsArg,
		   size_t maxobjectsArg,
		   off_t maxobjectsizeArg,
		   size_t chunksizeArg,
		   size_t maxsubsArg)
	: maxgets(maxgetsArg),
	  maxobjects(maxobjectsArg),
	  maxobjectsize(maxobjectsizeArg),
	  chunksize(chunksizeArg),
	  maxsubs(maxsubsArg)
{
}

userinit::~userinit()
{
}

STASHER_NAMESPACE_END

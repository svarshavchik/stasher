/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include <x/destroycallbackflag.H>

#include "threadreportimpl2.H"

#include <sstream>

STASHER_NAMESPACE_START

#define threadreportimplObj threadreportimpl2Obj

threadreportimplObj::threadreportimplObj()
{
}

threadreportimplObj::~threadreportimplObj() noexcept
{
}

void threadreportimplObj::dispatch_report(const x::ref<singlethreadreportObj> &rep,
					  const x::ptr<x::obj> &mcguffin)
{
	std::ostringstream o;

	rep->name=objname();

	try {
		rep->name=report(o);
	} catch (const x::exception &e)
	{
		o << std::endl << "Exception: " << e << std::endl;
	}

	rep->report=o.str();
}

x::ptr<singlethreadreportObj> threadreportimplObj::debugGetReport()

{
	x::ptr<singlethreadreportObj>
		rep=x::ptr<singlethreadreportObj>::create();
	x::ptr<x::obj> mcguffin=x::ptr<x::obj>::create();

	report(rep, mcguffin);

	x::destroyCallbackFlag cb=x::destroyCallbackFlag::create();

	mcguffin->ondestroy([cb]{cb->destroyed();});

	mcguffin=x::ptr<x::obj>();

	cb->wait();
	return rep;
}
STASHER_NAMESPACE_END

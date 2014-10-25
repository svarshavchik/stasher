/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include <x/destroycallbackflag.H>

#include "threadreportimpl.H"

#include <sstream>

STASHER_NAMESPACE_START

#include "threadreportimpl.msgs.def.H"

threadreportimplObj::threadreportimplObj()
{
}

threadreportimplObj::~threadreportimplObj() noexcept
{
}

void threadreportimplObj::dispatch(const report_msg &msg)
{
	std::ostringstream o;

	msg.rep->name=objname();

	try {
		msg.rep->name=report(o);
	} catch (const x::exception &e)
	{
		o << std::endl << "Exception: " << e << std::endl;
	}

	msg.rep->report=o.str();
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

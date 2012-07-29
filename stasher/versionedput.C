/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "stasher/versionedput.H"
#include <x/mpobj.H>
#include <x/destroycallbackflag.H>

STASHER_NAMESPACE_START

putresults versioned_put(const client &cl,
			 const client::base::transaction &tr,
			 const versionscollected &vers)
{
	typedef x::mpobj<putresults> retval_t;

	retval_t retval(putresults::create());

	{
		x::destroyCallbackFlag::base::guard guard;

		x::ref<x::obj> mcguffin=x::ref<x::obj>::create();

		guard(mcguffin);

		versioned_put_request(cl, tr,
				      [&retval, mcguffin]
				      (const putresults &res)
				      {
					      *retval_t::lock(retval)=res;
				      }, vers);
	}
	return *retval_t::lock(retval);
}

STASHER_NAMESPACE_END

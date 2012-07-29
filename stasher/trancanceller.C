/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "trancanceller.H"
#include "trandistributor.H"
#include "tobjrepo.H"

LOG_CLASS_INIT(trancancellerObj);

void trancancellerObj::installed(const std::string &objname,
				 const x::ptr<x::obj> &lock)

{
	if (objname.substr(0, hier_pfix.size()) == hier_pfix)
	{
		try {
			distributor->completed(x::uuid(objname.substr(hier_pfix
								      .size())
						       ));
		} catch (const x::exception &e)
		{
			LOG_FATAL(objname << ":" << e);
		}
	}
}

void trancancellerObj::removed(const std::string &objname,
			       const x::ptr<x::obj> &lock)

{
}

trancancellerObj::trancancellerObj(const x::ptr<trandistributorObj>
				   &distributorArg,
				   const std::string &nodename)
 : distributor(distributorArg),
			      hier_pfix(std::string(tobjrepoObj::done_hier)
					+ "/" + nodename + "/")
{
}

trancancellerObj::~trancancellerObj() noexcept
{
}

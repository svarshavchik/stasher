/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef userputreply_H
#define userputreply_H

#include <x/exception.H>
#include <x/uuid.H>

#include "stasher/namespace.H"
#include "stasher/reqstat.H"

STASHER_NAMESPACE_START

//! Response to a userput

class userputreply {

public:

	//! Original request uuid
	x::uuid uuid;

	//! Transaction status
	req_stat_t status;

	//! UUID of new/updated objects
	x::uuid newobjuuid;

	//! Default constructor

	userputreply() {}

	//! Constructor

	userputreply(const x::uuid &uuidArg,
		     req_stat_t statusArg,
		     const std::string &errmsgArg)
		: uuid(uuidArg),
		  status(statusArg)
	{
	}

	//! Constructor
	userputreply(const x::uuid &uuidArg,
		     req_stat_t statusArg,
		     const x::uuid &newobjuuidArg)
		: uuid(uuidArg),
		  status(statusArg),
		  newobjuuid(newobjuuidArg)
	{
	}

	//! Serialization function
	template<typename iter_type>
	void serialize(iter_type &iter)
	{
		iter(uuid);
		iter(status);
		iter(newobjuuid);
	}
};

STASHER_NAMESPACE_END
#endif

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef trandistcancel_H
#define trandistcancel_H

#include <set>
#include <x/uuid.H>

//! Message from a distributor peer, I am cancelling these transactions.

class trandistcancel {

public:
	//! The list of uuids

	std::set<x::uuid> uuids;

	//! Serialization function
	template<typename iter_type> void serialize(iter_type &iter)

	{
		iter(uuids);
	}
};

#endif

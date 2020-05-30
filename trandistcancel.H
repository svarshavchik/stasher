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
	template<typename ptr_type, typename iter_type>
	static void serialize(ptr_type ptr, iter_type &iter)
	{
		iter(ptr->uuids);
	}
};

#endif

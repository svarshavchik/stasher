/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef trandistihave_H
#define trandistihave_H

#include <set>
#include <x/uuid.H>

//! Message from a distributor peer, I already have these uuids from you

class trandistihave {

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

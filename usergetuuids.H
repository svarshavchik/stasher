/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef usergetuuids_H
#define usergetuuids_H

#include <x/uuid.H>
#include <set>
#include <string>
#include "stasher/serobjname.H"

STASHER_NAMESPACE_START

//! Client request to retrieve uuids of objects

class usergetuuids {

	//! Maximum number of objects that can be deserialized

	size_t limit;

public:
	//! This request's uuid;

	x::uuid requuid;

	//! The request parameters are kept in ptr-counted object, for speed

	class reqinfoObj : virtual public x::obj {

	public:
		//! Objects whose uuids should be retrieved

		std::set<serobjname> objects;

		//! Return objects' contents
		bool openobjects;

		//! Return without waiting for a quorum
		bool admin;

		reqinfoObj();
		~reqinfoObj();
	};

	//! Request parameters

	x::ptr<reqinfoObj> reqinfo;

	//! Default constructor

	usergetuuids();

	//! Constructor for deserializing

	usergetuuids(size_t limitArg);

	//! Destructor
	~usergetuuids();

	//! Serialization function
	template<typename ptr_type, typename iter_type>
	static void serialize(ptr_type ptr, iter_type &iter)
	{
		iter(ptr->requuid);
		iter(ptr->reqinfo->openobjects);
		iter(ptr->reqinfo->admin);
		iter(ptr->reqinfo->objects, ptr->limit);
	}
};

STASHER_NAMESPACE_END

#endif

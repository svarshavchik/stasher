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
		~reqinfoObj() noexcept;
	};

	//! Request parameters

	x::ptr<reqinfoObj> reqinfo;

	//! Default constructor

	usergetuuids();

	//! Constructor for deserializing

	usergetuuids(size_t limitArg);

	//! Destructor
	~usergetuuids() noexcept;

	//! Serialization function
	template<typename iter_type>
	void serialize(iter_type &iter)
	{
		iter(requuid);
		iter(reqinfo->openobjects);
		iter(reqinfo->admin);
		iter(reqinfo->objects, limit);
	}
};

STASHER_NAMESPACE_END

#endif

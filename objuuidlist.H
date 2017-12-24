/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objuuidlist_H
#define objuuidlist_H

#include <x/property_value.H>
#include <x/uuid.H>
#include <x/obj.H>
#include <x/ref.H>

#include <map>
#include <set>

class objwriterObj;

//! A list of UUIDs of some objects in a repository

//! This list is usually used when copying one repository into another one.

class objuuidlistObj : virtual public x::obj {

public:

	//! How many UUIDs, at a time, are transmitted

	static x::property::value<size_t> default_chunksize;

	objuuidlistObj() noexcept;
	~objuuidlistObj();

	//! The UUIDs of objects in the repository

	std::map<std::string, x::uuid> objuuids;

	//! Objects that do not exist in the repository
	std::set<std::string> objnouuids;

	//! Serialization function

	template<typename iter_type> void serialize(iter_type &iter)

	{
		iter(objuuids);
		iter(objnouuids);
	}
};

//! A reference to a objuuidlistObj

typedef x::ptr<objuuidlistObj> objuuidlist;

#endif

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_retrobj_H
#define stasher_retrobj_H

#include <x/uuid.H>
#include <x/ref.H>
#include <x/fd.H>

#include <stasher/namespace.H>

STASHER_NAMESPACE_START

//! A retrieved object

//! The retrieved object's uuid, and a readonly file descriptor with the
//! object's contents.

class retrobj {

public:
	//! Object's uuid
	x::uuid uuid;

	//! Object
	x::fdptr fd;

	//! Default constructor
	retrobj() {}

	//! Contructor
	retrobj(const x::uuid &uuidArg)
 : uuid(uuidArg) {}

	//! Destructor
	~retrobj() noexcept
	{
	}

	//! The serialization function

	//! Only the uuid gets serialized. The file descriptor is handled
	//! elsewhere.
	template<typename iter_type>
	void serialize(iter_type &iter)
	{
		iter(uuid);
	}
};

STASHER_NAMESPACE_END

#endif

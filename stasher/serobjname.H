/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_serobjname_H
#define stasher_serobjname_H

#include <string>

#include <stasher/namespace.H>

STASHER_NAMESPACE_START

template<bool serializing> class serdeserobjname;

//! Serialize an object name

template<>
class serdeserobjname<true> {

public:

	//! Implementation function
	template<typename iter_type>
	static void serialize(iter_type &iter,
			      const std::string &n)
	{
		iter(n);
	}
};

//! A name of an object in the repository

//! This is a std::string. The value-added is the custom serialization function
//! that enforces maximumobject name size when deserializing it.

class serobjname : public std::string {

public:

	//! Constructor
	serobjname(const std::string &s) : std::string(s) {}

	//! Default constructor

	serobjname() {}

	//! Serialization function

	template<typename ptr_type, typename iter_type>
	static void serialize(ptr_type ptr, iter_type &iter)
	{
		serdeserobjname<iter_type::serializing>::serialize(iter, *ptr);
	}
};

STASHER_NAMESPACE_END

#endif

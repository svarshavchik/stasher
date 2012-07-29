/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef deserobjname_H
#define deserobjname_H

#include <string>

#include "objrepo.H"
#include "stasher/serobjname.H"

STASHER_NAMESPACE_START

template<bool serializing> class serdeserobjname;

//! Serialize an object name

template<>
class serdeserobjname<false> {

public:

	//! Deserialization function

	template<typename iter_type>
	static void serialize(iter_type &iter,
			      std::string &n)
	{
		iter(n, objrepoObj::maxnamesize);
	}
};

STASHER_NAMESPACE_END

#endif

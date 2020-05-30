/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_userhelo_H
#define stasher_userhelo_H

#include <x/exception.H>
#include <stasher/userinit.H>

STASHER_NAMESPACE_START

//! Initial response from the server to a new client connection

class userhelo {

public:
	//! Node name

	std::string nodename;

	//! Cluster name

	std::string clustername;

	//! Connection limits

	userinit limits;

	//! Constructor

	userhelo();

	//! Constructor

	userhelo(const userinit &limitsArg);

	//! Destructor

	~userhelo();

	//! Serialization function

	template<typename ptr_type, typename iter_type>
	static void serialize(ptr_type ptr, iter_type &iter)
	{
		iter(ptr->nodename);
		iter(ptr->clustername);
		ptr->limits.serialize(&ptr->limits, iter);
	}
};

STASHER_NAMESPACE_END

#endif

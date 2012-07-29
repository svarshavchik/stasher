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

	~userhelo() noexcept;

	//! Serialization function

	template<typename iter_type>
	void serialize(iter_type &iter)
	{
		iter(nodename);
		iter(clustername);
		limits.serialize(iter);
	}
};

STASHER_NAMESPACE_END

#endif

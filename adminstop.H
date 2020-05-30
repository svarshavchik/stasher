/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef adminstop_H
#define adminstop_H

#include <x/exception.H>

#include "stasher/namespace.H"

STASHER_NAMESPACE_START

//! Command: server shutdown

class adminstop {

public:
	//! Serialization function
	template<typename ptr_type, typename iter_type>
	static void serialize(ptr_type ptr, iter_type &iter)
	{
	}
};

STASHER_NAMESPACE_END

#endif

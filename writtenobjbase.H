/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef writtenobjbase_H
#define writtenobjbase_H

#include <x/obj.H>
#include <x/exception.H>

#include "stasher/namespace.H"

STASHER_NAMESPACE_START

class objwriterObj;

//! Base class for objects written by objwriterthreadObj

class writtenobjbaseObj : virtual public x::obj {

public:
	writtenobjbaseObj() noexcept;
	~writtenobjbaseObj();

	//! The serialization function must be implemented in a subclass

	//! The subclass should invoke writer.serialize(obj) to serialize
	//! objects.

	virtual void serialize(objwriterObj &writer)=0;
};

STASHER_NAMESPACE_END

#endif

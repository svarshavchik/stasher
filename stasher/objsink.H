/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objsink_H
#define objsink_H

#include <x/ref.H>
#include <x/obj.H>

#include "stasher/namespace.H"

STASHER_NAMESPACE_START

class writtenobjbaseObj;

//! An interface for an object sink

//! An object "sink" implements a write() function that takes a reference to
//! a writtenobjbaseObj, and does something with it.

class objsinkObj : virtual public x::obj {

public:
	//! Constructor

	objsinkObj();

	//! Destructor
	~objsinkObj() noexcept;

	//! The object sink
	virtual void write(const x::ref<writtenobjbaseObj> &object)=0;
};

//! A reference to an object sink

typedef x::ref<objsinkObj> objsink;

STASHER_NAMESPACE_END

#endif

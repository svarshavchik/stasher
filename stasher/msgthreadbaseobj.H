/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef msgthreadbaseobj_H
#define msgthreadbaseobj_H

#include <x/obj.H>

//! Base superclass for incoming messages to a class.

//! The superclass defines a go() method. The subclass then performs the
//! necessary action for the respective class.
//!
//! This superclass is used for sending messages to fdobjreaderthreadObj
//! instance.

template<typename obj_type> class msgthreadbaseObj : virtual public x::obj {

public:
	msgthreadbaseObj() {}
	~msgthreadbaseObj() {}

	//! Invoke the message
	virtual void go(obj_type &cl)=0;
};

#endif

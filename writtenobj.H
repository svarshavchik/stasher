/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef writtenobj_H
#define writtenobj_H

#include "writtenobjbase.H"
#include <x/ref.H>

STASHER_NAMESPACE_START

//! A template for a writeable message based on a class with a default constructor

//! The template parameter is a class with a default constructor.
//! writtenObj<msg_type>::ref_t gives a reference to a class that's derived
//! from writtenobjbaseObj, contains a member called 'msg' that's the instance
//! of the template class, and implements a serialize() method that passes
//! 'msg' to objwriterObj's serialize().

template<typename msg_type> class writtenObj : public writtenobjbaseObj {
public:
	//! Destructor
	~writtenObj() {}

	//! The instance of the class
	msg_type msg;

	//! A template for constructing this object.

	template<typename... Arg_t>
	writtenObj(Arg_t && ...args) : msg(std::forward<Arg_t>(args)...)
	{
	}

	//! The serialize() method
	void serialize(objwriterObj &writer) override
	{
		writer.serialize( msg );
	}

	//! A reference to this object.
	typedef x::ref< writtenObj<msg_type> > ref_t;
};

STASHER_NAMESPACE_END

#endif

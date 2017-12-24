/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objsource_H
#define objsource_H

#include <x/deserialize.H>
#include <x/obj.H>
#include <x/stopexception.H>

#include "stasher/namespace.H"

STASHER_NAMESPACE_START

//! Object sourcerer

//! This is a somewhat analogous opposite of objsinkObj, except that its
//! implemented as a template function that deserializes a stream of objects
//! which get passed to a handler.
//!
//! \par Parameters
//!
//! objsource() takes three parameters:
//!
//! - a beginning iterator and an ending iterator that define some input
//! sequence.
//!
//! - a handler object that implements the deserialization any API. [DESER]
//!
//! \par Deserialization
//!
//! objsource runs a loop that iterates until the beginning iterator
//! compares equal to the ending iterator. If it's not, the next object gets
//! deserialized from the input sequence and passed to the handler object.
//!
//! objsource() returns normally if the beginning iterator compares equal to
//! the ending iterator, or if catches a x::stopexception thrown from the
//! handler or the iterators. In either case, objsource() returns normally.
//! [RETURN]

template<typename iter_type,
	 typename handler_type>
void objsource(iter_type &beg_iter, //!< Beginning iterator
	       iter_type &end_iter, //!< Ending iterator
	       handler_type &handlerobj) //!< Object handler

{
	typedef typename x::deserialize::iterator<iter_type> deser_iter_t;

	typedef typename deser_iter_t::template any<handler_type> any_t;

	try {
		deser_iter_t deser(beg_iter, end_iter);
		any_t any(deser, handlerobj);

		while (beg_iter != end_iter)
			any();
	} catch (const x::stopexception &e)
	{
		;
	}
}

STASHER_NAMESPACE_END

#endif

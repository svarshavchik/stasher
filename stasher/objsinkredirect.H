/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objsinkredirect_H
#define objsinkredirect_H

#include "objsink.H"

STASHER_NAMESPACE_START

//! An implementation of an object sink that forwards messages to another sink

//! This is useful when a message producer maintains a weak reference on its
//! sink, and harakiri-s itself when the sink goes away. Something else would
//! hold a reference to this sink, use it with the producer, and terminate
//! producer's output by destroying the redirect sink.

class objsinkredirectObj : public objsinkObj {

	//! The destination sink

	objsink sink;

public:
	//! Constructor

	objsinkredirectObj(const objsink &sinkArg);
	~objsinkredirectObj();

	//! Sink a message
	void write(const x::ref<writtenobjbaseObj> &object);

};

//! A reference to an object sink redirector

typedef x::ref<objsinkredirectObj> objsinkredirect;

STASHER_NAMESPACE_END

#endif

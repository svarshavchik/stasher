/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef singlethreadreport_H
#define singlethreadreport_H

#include <string>
#include <x/obj.H>
#include <stasher/namespace.H>
#include <thread>

STASHER_NAMESPACE_START

//! A thread reports on what it's doing, in here

class singlethreadreportObj : virtual public x::obj {

public:
	singlethreadreportObj() {}
	~singlethreadreportObj() noexcept {}

	//! Thread id
	std::thread::id thread_id;

	//! What the thread calls itself
	std::string name;

	//! Filled in by the thread
	std::string report;
};

STASHER_NAMESPACE_END

#endif

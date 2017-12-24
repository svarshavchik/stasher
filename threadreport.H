/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef threadreport_H
#define threadreport_H

#include "singlethreadreport.H"
#include <x/ref.H>
#include <vector>
#include <iostream>

STASHER_NAMESPACE_START

//! A report from all threads

//! A reference to this object gets returned from stoppableThreadTrackerImplObj::getReport()
//!
//! This method sends a request to all threads and returns immediately.
//! Remove the mcguffin, and add a destructor callback to it. When the
//! mcguffin goes out of scope, the report is ready.

class threadreportObj : virtual public x::obj {

public:
	//! Constructor
	threadreportObj() {}

	//! Destructor
	~threadreportObj() {};

	//! Reports being prepared
	std::vector<x::ref<singlethreadreportObj> > reports;

	//! When this mcguffin goes out of scope, the reports are done.
	x::ptr<x::obj> mcguffin;

	//! Once the report is done, sort the reports vector by name and format it into a stream

	void format(std::ostream &o);

	class sort_threads;
};

STASHER_NAMESPACE_END

#endif

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef threadreportimpl_H
#define threadreportimpl_H

#include "singlethreadreport.H"
#include <iostream>
#include <x/eventqueuemsgdispatcher.H>
#include <x/obj.H>

STASHER_NAMESPACE_START
//! A superclass for threads that implement a report() on its status

class threadreportimplObj : virtual public x::obj,
			    virtual public x::eventqueuemsgdispatcherObj {


public:
	threadreportimplObj();
	~threadreportimplObj() noexcept;

	//! Obtain a report from the thread for debugging purposes
	x::ptr<STASHER_NAMESPACE::singlethreadreportObj>
	debugGetReport();

	//! Subclass implements getReport()

	//! \return the name of the thread. The thread's status report should
	//! be written to the given output stream.

	virtual std::string report(std::ostream &rep)=0;

#include "threadreportimpl.msgs.decl.H"

};

STASHER_NAMESPACE_END

#endif

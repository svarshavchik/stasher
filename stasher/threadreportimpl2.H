/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef threadreportimpl2_H
#define threadreportimpl2_H

#include "singlethreadreport.H"
#include <iostream>
#include <x/threadmsgdispatcher.H>
#include <x/obj.H>

STASHER_NAMESPACE_START
//! A superclass for threads that implement a report() on its status

class threadreportimpl2Obj : virtual public x::obj,
			    virtual public x::threadmsgdispatcherObj {


public:
	threadreportimpl2Obj();
	~threadreportimpl2Obj() noexcept;

	//! Obtain a report from the thread for debugging purposes
	x::ptr<STASHER_NAMESPACE::singlethreadreportObj>
	debugGetReport();

	//! Subclass implements getReport()

	//! \return the name of the thread. The thread's status report should
	//! be written to the given output stream.

	virtual std::string report(std::ostream &rep)=0;

	virtual void foo()=0;

#include "threadreportimpl2.msgs.H"

};

STASHER_NAMESPACE_END

#endif

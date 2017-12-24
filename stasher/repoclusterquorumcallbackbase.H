/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_repoclusterquorumcallbackbase_H
#define stasher_repoclusterquorumcallbackbase_H

#include <x/obj.H>
#include <x/exception.H>

#include <stasher/namespace.H>
#include <stasher/quorumstate.H>

STASHER_NAMESPACE_START

//! A Callback that gets invoked when the cluster's quorum changes

class repoclusterquorumcallbackbaseObj : virtual public x::obj {

public:
	//! Constructor
	repoclusterquorumcallbackbaseObj();

	//! Destructor
	~repoclusterquorumcallbackbaseObj();

	//! Callback
	virtual void quorum(const quorumstate &quorum)=0;
};

STASHER_NAMESPACE_END

#endif

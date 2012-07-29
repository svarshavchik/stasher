/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef trandistreceived_H
#define trandistreceived_H

#include <x/exception.H>
#include "trancancelleduuid.H"
#include "trandistuuidfwd.H"
#include "trandistreceivedfwd.H"

//! An interface for changes to a set of transaction IDs being distributed.

class trandistreceivedObj : public trancancelleduuidObj {

public:
	//! Constructor
	trandistreceivedObj();

	//! Destructor
	~trandistreceivedObj() noexcept;

	//! Distributed a bunch of UUIDs
	virtual void received(const trandistuuid &uuids)=0;

};

#endif


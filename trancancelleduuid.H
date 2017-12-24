/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef trancancelleduuid_H
#define trancancelleduuid_H

#include <x/obj.H>
#include <x/exception.H>
#include "tranuuidfwd.H"

//! An interface for cancelling transactions uuids from some set

class trancancelleduuidObj : virtual public x::obj {

public:
	//! Constructor
	trancancelleduuidObj();

	//! Destructor
	~trancancelleduuidObj();

	//! Remove a bunch of uuids.
	virtual void cancelled(const tranuuid &uuids)=0;
};

#endif

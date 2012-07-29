/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef tranreceived_H
#define tranreceived_H

#include <x/exception.H>
#include "trancancelleduuid.H"
#include "tranuuidfwd.H"
#include "tranreceivedfwd.H"

//! An interface for changes to a set of transaction IDs.

class tranreceivedObj : public trancancelleduuidObj {

public:
	//! Constructor
	tranreceivedObj();

	//! Destructor
	~tranreceivedObj() noexcept;

	//! Add a bunch of uuids.
	virtual void received(const tranuuid &uuids)=0;

};

#endif


/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef tranuuid_H
#define tranuuid_H

#include <x/uuid.H>
#include <x/obj.H>
#include <x/exception.H>

#include <set>

#include "tranreceived.H"
#include "tranuuidfwd.H"

//! A set of transactions that were received or cancelled by a node.

class tranuuidObj : public tranreceivedObj {

public:
	//! Constructor
	tranuuidObj();

	//! Destructor
	~tranuuidObj();

	//! The set
	std::set<x::uuid> uuids;

	//! Add a bunch of uuids.
	void received(const tranuuid &recvuuids) override;

	//! Remove a bunch of uuids
	void cancelled(const tranuuid &cancuuids) override;

	//! Dump this object
	void to_string(std::ostream &o);

};

#endif

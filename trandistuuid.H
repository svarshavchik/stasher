/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef trandistuuid_H
#define trandistuuid_H

#include <x/uuid.H>
#include <x/obj.H>
#include <x/exception.H>

#include <map>

#include "tranuuid.H"
#include "trandistreceived.H"

#include "trandistuuidfwd.H"

//! A particular transaction was received by some node.

class dist_received_status_t {

public:
	//! Whether the transaction was succesfully received
	short status;

	//! Which node the transaction was received from
	std::string sourcenode;

	//! Constructor
	dist_received_status_t(short statusArg,
			       const std::string &sourcenodeArg) noexcept
		: status(statusArg), sourcenode(sourcenodeArg) {}

	//! Default constructor
	dist_received_status_t() noexcept : status(0) {}

	//! Serialization function

	template<typename ptr_type, typename iter_type>
	static void serialize(ptr_type ptr, iter_type &iter)
	{
		iter(ptr->status);
		iter(ptr->sourcenode);
	}
};

//! Transaction received status: received without any issues
static constexpr short dist_received_status_ok=0;

//! Transaction received status: could not receive the transaction
static constexpr short dist_received_status_err=1;

//! A set of transactions that were received or cancelled by a node.

class trandistuuidObj : public trandistreceivedObj {

public:
	//! Constructor
	trandistuuidObj();

	//! Destructor
	~trandistuuidObj();

	//! The set
	std::map<x::uuid, dist_received_status_t> uuids;

	//! Add a bunch of uuids.
	void received(const trandistuuid &recvuuids) override;

	//! Remove a bunch of uuids
	void cancelled(const tranuuid &cancuuids) override;

	//! Dump this object
	void to_string(std::ostream &o);
};

#endif

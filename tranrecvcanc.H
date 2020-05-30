/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef tranrecvcanc_H
#define tranrecvcanc_H

#include "trandistuuid.H"

//! Peer transaction announcements

//! A peer sends this message to announce which transaction(s) it received
//! from other peers, and when it's notified by a peer that a transaction
//! has been cancelled.

class tranrecvcanc {

public:
	//! This message is intended for this master
	std::string mastername;

	//! This message is intended for a master with this serial number
	x::uuid masteruuid;

	//! Transactions that have been received, as a convenient tranuuid object.
	trandistuuid received;

	//! Transactions that have been cancelled, as a convenient tranuuid object
	tranuuid cancelled;

	//! Serialization function

	template<typename ptr_type, typename iter_type>
	static void serialize(ptr_type ptr, iter_type &iter)
	{
		iter(ptr->mastername);
		iter(ptr->masteruuid);

		iter(ptr->received->uuids);
		iter(ptr->cancelled->uuids);
	}

	//! Constructor

	//! The constructor instantiates received and cancelled
	//!
	tranrecvcanc();

	//! Constructor

	//! The constructor instantiates received and cancelled
	//!
	tranrecvcanc(const std::string &masternameArg,
		     const x::uuid &masteruuidArg);

	//! Destructor
	~tranrecvcanc();

};
#endif

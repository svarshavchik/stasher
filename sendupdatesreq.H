/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef sendupdatesreq_H
#define sendupdatesreq_H

#include "stasher/namespace.H"

STASHER_NAMESPACE_START

//! Client message to server to enable updates of the server's status

//! The server begins sending various update messages:
//!
//! updatedquorumstatus
//!

class sendupdatesreq {

public:
	bool wantupdates; //!< Whether to enable or disable update messages

	//! Constructor
	sendupdatesreq() {}

	//! Constructor
	sendupdatesreq(bool wantupdatesArg)
		: wantupdates(wantupdatesArg)
	{
	}

	//! Destructor
	~sendupdatesreq() {}

	//! The serialization function
	template<typename ptr_type, typename iter_type>
	static void serialize(ptr_type ptr, iter_type &iter)
	{
		iter(ptr->wantupdates);
	}
};

STASHER_NAMESPACE_END

#endif

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_userinit_H
#define stasher_userinit_H

#include <x/exception.H>
#include <unistd.h>
#include <stasher/namespace.H>

STASHER_NAMESPACE_START

//! Initial response from the server, informing the clients of its limits

class userinit {

public:
	//! Maximum number of concurrent get() requests that may be sent to the server

	//! If the client has more concurrent get() requests, they
	//! get queued up on the client side, and sent to the server after it
	//! finishes processing the pending get() requests.
	//!
	//! Internal setting.
	size_t maxgets;

	//! Maximum number of objects allowed in a single transaction
	size_t maxobjects;

	//! Maximum aggregate size of objects in a single transaction
	off_t maxobjectsize;

	//! Maximum chunk size, when transferring object contents

	//! Internal setting.
	size_t chunksize;

	//! Maximum number of subscriptions allowed
	size_t maxsubs;

	//! Default constructor

	userinit();

	//! Constructor

	userinit(size_t maxgetsArg,
		 size_t maxobjectsArg,
		 off_t maxobjectsizeArg,
		 size_t chunksizeArg,
		 size_t maxsubsArg);

	//! Destructor

	~userinit() noexcept;

	//! Serialization function
	template<typename iter_type>
	void serialize(iter_type &iter)
	{
		iter(maxgets);
		iter(maxobjects);
		iter(maxobjectsize);
		iter(chunksize);
		iter(maxsubs);
	}
};

STASHER_NAMESPACE_END

#endif

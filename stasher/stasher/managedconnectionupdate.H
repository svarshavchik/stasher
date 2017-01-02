/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_managedconnectionupdate_H
#define stasher_managedconnectionupdate_H

#include <stasher/namespace.H>
#include <stasher/client.H>

STASHER_NAMESPACE_START

//! A callback from a managed subscription or server status, that reports the status of the connection.

class managedconnectionupdateObj : virtual public x::obj {

public:
	managedconnectionupdateObj();

	~managedconnectionupdateObj();

	//! Update the connection status

	virtual void connection_update(//! The current status of the connection

				       //! - req_disconnected_stat:
				       //! disconnected from the server
				       //!
				       //! - req_processed_stat: connected to
				       //! the server.
				       //!
				       //! Note that a \c req_processed_stat
				       //! also gets sent when a subscription
				       //! gets established.

				       req_stat_t status)=0;
};

STASHER_NAMESPACE_END

#endif

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_managedsubscriber_H
#define stasher_managedsubscriber_H

#include <stasher/namespace.H>
#include <stasher/client.H>
#include <stasher/managedconnectionupdate.H>

STASHER_NAMESPACE_START

//! A subscription that's managed by a \ref manager "connection manager".

//! This object inherits from
//! \ref clientObj::subscriberObj "client::base::subscriberObj" and adds a
//! callback that reports the status of the subscription request.

class managedsubscriberObj : public clientObj::subscriberObj,
			     public managedconnectionupdateObj {

public:
	managedsubscriberObj();

	~managedsubscriberObj();
};

//! A reference to a managed subscription
typedef x::ref<managedsubscriberObj> managedsubscriber;

//! A nullable pointer reference to a managed subscription
typedef x::ptr<managedsubscriberObj> managedsubscriberptr;

STASHER_NAMESPACE_END

#endif

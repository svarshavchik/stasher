/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef serverstatuscallback_H
#define serverstatuscallback_H

#include <x/obj.H>
#include <x/ref.H>
#include <x/ptr.H>
#include <stasher/clusterstate.H>
#include <stasher/namespace.H>

STASHER_NAMESPACE_START

class userhelo;

//! Server status callback

//! Subclass from this object, and invoke
//! \ref clientObj::subscribeserverstatus "client->subscribeserverstatus()"
//! to be notified of changes to the object repository cluster's quorum status.

class serverstatuscallbackObj : virtual public x::obj {

public:
	//! Constructor
	serverstatuscallbackObj();

	//! Destructor
	~serverstatuscallbackObj();

	//! Callback upon registration, reports server connection details
	virtual void serverinfo(const userhelo &serverinfo)=0;

	//! Cluster's state has changed
	virtual void state(//! Cluster's quorum has changed
			   const clusterstate &state)=0;
};

//! A reference to a server status callback object
typedef x::ref<serverstatuscallbackObj> serverstatuscallback;

//! A nullable pointer to a server status callback object
typedef x::ptr<serverstatuscallbackObj> serverstatuscallbackptr;

STASHER_NAMESPACE_END

#endif

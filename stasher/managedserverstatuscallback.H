/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_managedserverstatuscallback_H
#define stasher_managedserverstatuscallback_H

#include <stasher/namespace.H>
#include <stasher/serverstatuscallback.H>
#include <stasher/managedconnectionupdate.H>

STASHER_NAMESPACE_START

//! A server status callback managed by a \ref manager "connection manager".

//! This object inherits from
//! \ref serverstatuscallbackObj "serverstatuscallbackObj"
//! and \ref managedconnectionupdateObj.

class managedserverstatuscallbackObj
	: public serverstatuscallbackObj,
	  public managedconnectionupdateObj {

public:
	managedserverstatuscallbackObj();

	~managedserverstatuscallbackObj();
};

//! A reference to a managed server status callback
typedef x::ref<managedserverstatuscallbackObj> managedserverstatuscallback;

//! A nullable pointer reference to a managed server status callback
typedef x::ptr<managedserverstatuscallbackObj> managedserverstatuscallbackptr;

STASHER_NAMESPACE_END

#endif

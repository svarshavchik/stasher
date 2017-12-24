/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_managedobject_H
#define stasher_managedobject_H

#include <stasher/namespace.H>
#include <stasher/managedconnectionupdate.H>

#include <x/uuid.H>
#include <x/fd.H>

STASHER_NAMESPACE_START

//! Callback that received a monitored object's contents, each time it changes.

class managedobjectObj : public managedconnectionupdateObj {

public:

	//! Constructor
	managedobjectObj();

	//! Destructor
	~managedobjectObj();

	//! The object's new or updated contents.

	virtual void updated(//! Object's name
			     const std::string &objname,

			     //! The object's uuid.
			     const x::uuid &uuid,

			     //! The object's contents
			     const x::fd &contents)=0;

	//! The object has been removed
	virtual void removed(const std::string &objname)=0;
};

//! A reference to a managed subscription
typedef x::ref<managedobjectObj> managedobject;

//! A nullable pointer reference to a managed subscription
typedef x::ptr<managedobjectObj> managedobjectptr;

STASHER_NAMESPACE_END

#endif

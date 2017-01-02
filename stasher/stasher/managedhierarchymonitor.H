/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_managedhierarchymonitor_H
#define stasher_managedhierarchymonitor_H

#include <stasher/namespace.H>
#include <stasher/managedconnectionupdate.H>
#include <x/obj.H>
#include <x/uuid.H>
#include <x/logger.H>

#include <string>

STASHER_NAMESPACE_START

//! A callback used to monitor the contents of an object hierarchy

class managedhierarchymonitorObj : public managedconnectionupdateObj {

	//! The logger
	LOG_CLASS_SCOPE;

public:
	//! Internal implementation object.
	class LIBCXX_HIDDEN implObj;

	managedhierarchymonitorObj();
	~managedhierarchymonitorObj();

	//! Wipe the slate clean, the current contents of the directory follow
	virtual void begin()=0;

	//! Contents enumerated.
	virtual void enumerated()=0;

	//! New or updated object
	virtual void updated(const std::string &objname,
			     const x::uuid &objuuid)=0;

	//! Removed object
	virtual void removed(const std::string &objname)=0;
};

//! A reference to a hierarchy monitor callback object
typedef x::ref<managedhierarchymonitorObj> managedhierarchymonitor;

//! A nullable pointer reference to a hierarchy monitor callback object
typedef x::ptr<managedhierarchymonitorObj> managedhierarchymonitorptr;

STASHER_NAMESPACE_END

#endif

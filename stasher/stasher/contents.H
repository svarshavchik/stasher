/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_contents_H
#define stasher_contents_H

#include <stasher/namespace.H>
#include <stasher/retrobj.H>

#include <map>
#include <string>

STASHER_NAMESPACE_START

class contentsBase;

//! get() returns a \ref getresults "getresults" whose \c objects member is a reference to this object.

class contentsObj : virtual public x::obj,
		    public std::map<std::string, retrobj>{
	
public:
	//! The superclass type

	typedef std::map<std::string, retrobj> map_t;

	//! Flag - request succeeded
	bool succeeded;

	//! An error message, upon a failure, gets placed here
	std::string errmsg;

	contentsObj();
	~contentsObj();
};

//! A reference to the results of \ref clientObj::get "objclient->get"()->objects

typedef x::ref<contentsObj, contentsBase> contents;

//! A nullable pointer reference to the results of \ref clientObj::get "objclient->get"()->objects

typedef x::ptr<contentsObj, contentsBase> contentsptr;

//! Superclass for references to contentsObj

//! \see contents

class contentsBase : public x::ptrref_base {

public:
	//! A typedef for map_t

	typedef contentsObj::map_t map_t;
};

STASHER_NAMESPACE_END

#endif

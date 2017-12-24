/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef trancanceller_H
#define trancanceller_H

#include <x/obj.H>
#include <x/ref.H>
#include <x/logger.H>

#include "objrepo.H"

class trandistributorObj;

//! Notifier callback for completed transactions

//! When a commited or a cancelled transaction results in the object
//! being installed in tobjrepoObj::done_hier/{nodename}, invoke the
//! distributor's completed() method [TRANCANCELLER].
//!
//! After constructing this object, invoke objrepoObj::installNotifier().

class trancancellerObj : public objrepoObj::notifierObj {

	//! The logger
	LOG_CLASS_SCOPE;

	//! The distributor object
	x::ptr<trandistributorObj> distributor;

	//! My node name
	std::string hier_pfix;

public:
	//! Constructor
	trancancellerObj(const x::ptr<trandistributorObj> &distributorArg,
			 const std::string &nodename);


	//! Destructor
	~trancancellerObj();

	//! An object was just installed

	void installed(const std::string &objname,
		       const x::ptr<x::obj> &lock);


	//! An object was just removed
	void removed(const std::string &objname,
		     const x::ptr<x::obj> &lock);

};

#endif


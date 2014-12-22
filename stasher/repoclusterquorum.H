/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef repoclusterquorum_H
#define repoclusterquorum_H

#include <x/obj.H>
#include <x/vipobj.H>
#include <x/weaklist.H>

#include "stasher/repoclusterquorumcallbackbase.H"
#include "stasher/quorumstate.H"

//! Implement the semantics of a cluster's quorum status

//! When there's a change in the node's state, a new controller object gets
//! instantiated, and the existing controller object is told to wrap things
//! up, but for some time, both controller objects may exist.
//!
//! It's desirable to force the quorum status to false immediately, when a
//! new controller object comes up, and ignore any status changes reported
//! by the former controller object, until it goes away and a new object
//! comes up. This is implemented as follows.
//!
//! Each new controller object is constructed with a mcguffin, and the mcguffin
//! gets installed into this quorum status, which forces its state to
//! false [NEWCONTROLLERQUORUMFALSE].
//! When the new controller execution thread comes up, it drops the mcguffin.
//! And as long as this status object's internal mcguffin list contains at least
//! one non-null mcguffin, updates to the node status get ignored and it
//! the status remains complete non-quorum [CONTROLLERQUORUMFORCEFALSE].
//!
//! By definition quorum status updates for a new controller object could only
//! possibly come in once the previous controller object is gone for good,
//! because the successor controller object's execution thread gets started
//! only from its
//! \ref repocontrollerbaseObj "predecessor controller's destructor".
//! Therefore, there's no need to take any action when these mcguffins go
//! out of scope, it's ok to wait until the next controller comes up, and
//! with the mcguffins being code quorum status updates take effect again.
//! [CONTROLLERQUORUMRESUME].

class quorumstatus_impl {

public:

	//! The official value

	STASHER_NAMESPACE::quorumstate value;

	//! All mcguffins that are preventing the quorum from being reported.

	x::weaklist<x::obj> mcguffins;

	//! Constructor
	quorumstatus_impl();

	//! Destructor
	~quorumstatus_impl() noexcept;

	//! Report the quorum's current value

	operator const STASHER_NAMESPACE::quorumstate &() const noexcept
	{
		return value;
	}

	//! Check if there's at least one mcguffin still in existence.

	//! \return \c false if there's still one mcguffin, else statusArg.

	STASHER_NAMESPACE::quorumstate
	reported_status(const STASHER_NAMESPACE::quorumstate
			&statusArg);
};


//! The cluster's quorum status

//! This is a very important object.
//! If true, all slaves are presently connected with the master.

class repoclusterquorumObj
	: public x::vipobj<quorumstatus_impl,
			   const STASHER_NAMESPACE::quorumstate &>,
	  virtual public x::obj {
public:
	//! Constructor
	repoclusterquorumObj();

	//! Destructor
	~repoclusterquorumObj() noexcept;

	//! Convenience method to install a very important object handler

	void install(const x::ref<STASHER_NAMESPACE::
				  repoclusterquorumcallbackbaseObj> &notifier);


};

//! The base class for repoclusterquorumObj references

//! The base class import very important object lock types, for convenience.
//!
class repoclusterquorumrefbase : public x::ptrrefBase {

public:
	//! A read lock on the very important object that represents the quorum status
	typedef repoclusterquorumObj::readlock readlock;

	//! A write lock on the very important object that represents the quorum status
	typedef repoclusterquorumObj::writelock writelock;

	//! An update lock on the very important object that represents the quorum status

	typedef repoclusterquorumObj::updatelock updatelock;

	//! A handler lock on the very important object that represents the quorum status
	typedef repoclusterquorumObj::handlerlock handlerlock;
};

//! A reference to a cluster quorum object

typedef x::ptr<repoclusterquorumObj,
	       repoclusterquorumrefbase> repoclusterquorum;

#endif

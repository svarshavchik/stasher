/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_quorumstateobj_H
#define stasher_quorumstateobj_H

#include <x/obj.H>
#include <x/ref.H>

#include <stasher/namespace.H>
#include <stasher/quorumstate.H>

STASHER_NAMESPACE_START

//! A reference-counted quorums state object

class quorumstateObj : public quorumstate, virtual public x::obj {

public:
	//! Constructor

	quorumstateObj();

	//! Destructor

	~quorumstateObj();
};

//! A reference to a quorum state object

typedef x::ref<quorumstateObj> quorumstateref;

//! A pointer to a quorum state object

typedef x::ptr<quorumstateObj> quorumstateptr;

STASHER_NAMESPACE_END

#endif

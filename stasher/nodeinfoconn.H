/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef nodeinfostatus_H
#define nodeinfostatus_H

#include "stasher/nodeinfo.H"
#include <x/ref.H>
#include <x/weakptr.H>

class peerstatusObj;

//! Information about a node, and the current connection to the node.

//! Derived from nodeinfo, adds a weak pointer to the current connection
//! to the given node, and the node's status.

class nodeinfoconn : public STASHER_NAMESPACE::nodeinfo {

public:
	nodeinfoconn() noexcept {}
	~nodeinfoconn() noexcept {}

	//! Constructor
	nodeinfoconn(const nodeinfo &n) : nodeinfo(n) {}

	x::weakptr<x::ptr<peerstatusObj> > connection;
	//!< Connection to this node
};

#endif

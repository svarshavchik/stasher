/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_clusterstate_H
#define stasher_clusterstate_H

#include <set>
#include <x/exception.H>
#include <stasher/quorumstate.H>
#include <stasher/namespace.H>

STASHER_NAMESPACE_START

//! The status of the object repository cluster

//! Subclass clusterstate to include the current cluster node, and
//! the list of all nodes in the cluster.

class clusterstate : public quorumstate {

public:

	//! Current cluster master
	std::string master;

	//! List of nodes in the cluster
	std::set<std::string> nodes;

	//! Constructor
	clusterstate();

	//! Destructor
	~clusterstate();

	//! Serialization function

	template<typename ptr_type, typename iter_type>
	static void serialize(ptr_type ptr, iter_type &iter)
	{
		quorumstate::serialize(ptr, iter);
		iter(ptr->master);
		iter(ptr->nodes);
	}

	//! Helper operator

	clusterstate &operator=(const quorumstate &q)
	{
		quorumstate::operator=(q);
		return *this;
	}

	//! Comparison operator

	bool operator==(const clusterstate &o) const noexcept
	{
		return quorumstate::operator==(o) &&
			master == o.master && nodes == o.nodes;
	}

	//! Comparison operator

	bool operator!=(const clusterstate &o) const noexcept
	{
		return !operator==(o);
	}
};

STASHER_NAMESPACE_END

#endif

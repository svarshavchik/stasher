/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_quorumstate_H
#define stasher_quorumstate_H

#include <x/exception.H>
#include <x/locale.H>
#include <x/value_string.H>
#include <x/to_string.H>
#include <stasher/namespace.H>

STASHER_NAMESPACE_START

//! The cluster's quorum state/status

class quorumstate {

public:

	//! All cluster members are in quorum with master

	bool full;

	//! The master and its quorum slaves comprise the majority of the cluster

	bool majority;

	//! Constructor
	quorumstate();

	//! Constructor
	quorumstate(bool fullArg,
		    bool majorityArg);

	//! Destructor
	~quorumstate();

	//! Serialization function

	template<typename ptr_type, typename iter_type>
	static void serialize(ptr_type ptr, iter_type &iter)
	{
		iter(ptr->full);
		iter(ptr->majority);
	}

	//! Comparison operator

	bool operator==(const quorumstate &o) const noexcept
	{
		return full == o.full && majority == o.majority;
	}

	//! Comparison operator

	bool operator!=(const quorumstate &o) const noexcept
	{
		return !operator==(o);
	}

	//! Convert a class instance to a string.
	template<typename OutputIterator>
	OutputIterator to_string(OutputIterator iter,
				const x::const_locale &localeRef)
		const
	{
		std::ostringstream o;

		o << "full: " << x::to_string(full)
		  << ", majority: " << x::to_string(majority);

		std::string s(o.str());

		return std::copy(s.begin(), s.end(), iter);
	}
};

STASHER_NAMESPACE_END

#endif

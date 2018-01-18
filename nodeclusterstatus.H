/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef nodeclusterstatus_H
#define nodeclusterstatus_H

#include <string>
#include <x/uuid.H>
#include <x/ymdhms.H>
#include <x/exception.H>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <iterator>

//! The cluster status of a node

//! The cluster status defines the relationship of the node with the cluster,
//! and consists of the following items.
//!
//! - the name of the node's master. A node may name itself the master, if it
//! think it's the master of the cluster, or if it thinks it's some other node's
//! slave.
//!
//! - the node's UUID. When a node annoints itself its own master, it resets
//! the UUID, which is then replicated to its future slaves. [UUIDRESET]
//! A slave pledges allegiance to its master by copying master's
//! nodeclusterstatus. [UUIDREPLICATE]
//!
//! - A timestamp when the node became a master.
//!
//! - how many slaves the node's master has. If the node think it's the master,
//! how many slaves the master is connected to. If the node is a slave, this
//! count is told by the master to its slaves.
//!
//! \par Comparison [COMPARE]
//!
//! The class also defines comparison functions as follows: a status with
//! a higher slave count is considered "greater than" a status with a lower
//! slave count. If the slave counts are the same, the timestamps get compared
//! and the earliest timestamp wins. [OLDESTWINS]
//! In the unlikely event that the timestamps
//! are the same, the node names get compared.
//!
//! In all cases, the flag whether the node is reluctant to become master trumps
//! above all.

class nodeclusterstatus {
public:
	std::string master; //!< Node's master's name.

	x::uuid uuid;	//!< Master's UUID

	size_t slavecount; //!< The slave count.

	time_t timestamp; //!< The timestamp

	bool reluctant;	//!< Whether the master is reluctant to be one.

	//! Constructor
	nodeclusterstatus() noexcept : slavecount(0),
		timestamp(0), reluctant(false)
	{
	}

	//! Constructor
	nodeclusterstatus(const std::string &masterArg,
			  const x::uuid &uuidArg,
			  size_t slavecountArg,
			  bool reluctantArg) noexcept
		: master(masterArg), uuid(uuidArg), slavecount(slavecountArg),
		timestamp(0), reluctant(reluctantArg)
	{
	}

	~nodeclusterstatus() {}

	//! If this node is a master, check if another node is this node's slave

	bool ismasterof(const nodeclusterstatus &slave)
		const
	{
		return master == slave.master &&
			uuid == slave.uuid;
	}

	//! Comparison function

	int compare(const nodeclusterstatus &o) const
	{
		if (reluctant && !o.reluctant)
			return -1;

		if (!reluctant && o.reluctant)
			return 1;

		if (slavecount < o.slavecount)
			return -1;

		if (slavecount > o.slavecount)
			return 1;

		if (timestamp > o.timestamp)
			return -1;

		if (timestamp < o.timestamp)
			return 1;

		return master.compare(o.master);
	}

	//! Comparison operation

	bool operator==(const nodeclusterstatus &o) const
	{
		return compare(o) == 0;
	}

	//! Comparison operation

	bool operator!=(const nodeclusterstatus &o) const
	{
		return compare(o) != 0;
	}

	//! Comparison operation

	bool operator>(const nodeclusterstatus &o) const
	{
		return compare(o) > 0;
	}

	//! Comparison operation

	bool operator<(const nodeclusterstatus &o) const
	{
		return compare(o) < 0;
	}

	//! Comparison operation

	bool operator>=(const nodeclusterstatus &o) const
	{
		return compare(o) >= 0;
	}

	//! Comparison operation

	bool operator<=(const nodeclusterstatus &o) const
	{
		return compare(o) <= 0;
	}

	//! Serialization function

	template<typename iter_type> void serialize(iter_type &iter)

	{
		iter(master);
		iter(uuid);
		iter(slavecount);
		iter(timestamp);
		iter(reluctant);
	}

	//! Convert to a string representation

	void toString(std::ostream &o) const
	{
		x::uuid::charbuf cb;

		uuid.asString(cb);

		o << "master: " << master << " (uuid "
		  << cb << "), " << slavecount << " slaves, timestamp ";


		x::ymdhms localtimestamp(timestamp, x::tzfile::base::local());

		auto l=x::locale::base::global();

		o << x::tostring(localtimestamp.format("%Y-%m-%d %H:%M:%S %z"),
				 l);

		if (reluctant)
			o << ", inferior";
	}
};

#endif

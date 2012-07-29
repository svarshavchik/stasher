/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef mastersyncinfo_H
#define mastersyncinfo_H

#include <x/weakptr.H>
#include "repocontrollermaster.H"
#include "trandistuuidfwd.H"

//! Message to the peer connection object from a master controller

class mastersyncinfoObj : virtual public x::obj {

public:
	//! Constructor
	mastersyncinfoObj(const std::string &masternameArg,
			  const x::uuid &masteruuidArg,
			  const repocontrollermasterptr &controllerArg,
			  const x::ptr<repocontrollermasterObj
				       ::slaveConnectionObj> &connectionArg,
			  const trandistuuid &received_uuidsArg);


	//! Destructor
	~mastersyncinfoObj() noexcept;

	//! Master's name
	std::string mastername;

	//! Master's UUID
	x::uuid masteruuid;

	//! The master controller
	x::weakptr<repocontrollermasterptr> controller;

	//! The master's connection reference to the slave
	x::ptr<repocontrollermasterObj::slaveConnectionObj> connection;

	//! This object keeps track of transactions the peer received

	//! The connection object, on the master side, doesn't touch this
	//! object, but passes it back to its master controller object when
	//! the peer announces that it received or cancelled a transaction
	trandistuuid received_uuids;
};

//! A reference to a mastersyncinfoObj

typedef x::ptr<mastersyncinfoObj> mastersyncinfo;

#endif

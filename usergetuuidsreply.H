/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef usergetuuidsreply_H
#define usergetuuidsreply_H

#include <x/uuid.H>
#include <set>
#include <string>
#include "stasher/client.H"
#include "stasher/retrobj.H"

STASHER_NAMESPACE_START

//! Response to a client request to retrieve uuids of objects

class usergetuuidsreply {

public:

	//! To reduce copying, put the list of object names into a refobj

	class objvecObj : virtual public x::obj,
			  public std::vector<std::pair<std::string, retrobj> > {
	public:
		objvecObj() {}
		~objvecObj() {}
	};


	//! When requesting objects to be opened, this gets sent, followed by file descriptors, before usergetuuidsreply

	class openobjects {

	public:

		//! Request uuid

		x::uuid requuid;

		//! Default constructor
		openobjects()
		{
		}

		//! Constructor
		openobjects(const x::uuid &requuidArg)
 : requuid(requuidArg)
		{
		}

		//! Destructor
		~openobjects()
		{
		}

		//! Serialization function
		template<typename iter_type>
		void serialize(iter_type &iter)
		{
			iter(requuid);
		}
	};

	//! The original request's uuid;

	x::uuid requuid;

	//! uuids being returned
	x::ptr<objvecObj> uuids;

	//! Flag - request succeeded
	bool succeeded;

	//! An error message, upon a failure, gets placed here
	std::string errmsg;

	//! Internal object used to store a mcguffin

	//! On the server, the mcguffin for the semaphore that limits the number
	//! of concurrently open file descripts is placed here, so that the
	//! semaphore gets held until all the objects have been sent to the
	//! client
	x::ptr<x::obj> semaphore_mcguffin;

	//! Default constructor
	usergetuuidsreply();

	//! Constructor
	usergetuuidsreply(const x::uuid &requuidArg);

	//! Destructor
	~usergetuuidsreply();

	//! Serialization function
	template<typename iter_type>
	void serialize(iter_type &iter)
	{
		iter(requuid);
		iter(succeeded);
		iter(errmsg);
		iter(static_cast<std::vector<std::pair<std::string, retrobj> >
		     &>(*uuids));
	}
};

STASHER_NAMESPACE_END

#endif

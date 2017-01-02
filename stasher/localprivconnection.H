/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef localprivconnection_H
#define localprivconnection_H

#include "localconnection.H"
#include "repopeerconnection.H"
#include "adminstop.H"
#include "writtenobj.H"
#include "boolref.H"
#include "clusterlistenerfwd.H"

//! A local connection that can issue privileged commands

class localprivconnectionObj : public localconnectionObj {

	//! The listener thread

	clusterlistener listener;

public:
	class haltrequest_resp_msg_t;
private:
#include "localprivconnection.msgs.H"

public:
	//! Deserialization classlist enumerator

	template<typename iter_type>
	static void classlist(iter_type &iter)
	{
		localconnectionObj::classlist(iter);

		iter.template serialize<STASHER_NAMESPACE::adminstop,
					deser<STASHER_NAMESPACE::adminstop> >();

#include "localprivconnection.classlist.auto.H"
	}

	//! Constructor
	localprivconnectionObj(const std::string &threadnameArg,
			       const tobjrepo &repo,
			       const STASHER_NAMESPACE::stoppableThreadTracker &tracker,
			       const x::ptr<trandistributorObj> &distributor,
			       const repoclusterinfoimpl &cluster,
			       const spacemonitor &spacemonitorArg,
			       const x::semaphore &getsemaphoreArg,
			       const clusterlistener &listenerArg);


	//! Destructor
	~localprivconnectionObj();

	//! The actual main loop

	MAINLOOP_DECL;

public:

	//! Stop the node
	void deserialized(const STASHER_NAMESPACE::adminstop &msg);


#include "localprivconnection.deserialized.auto.H"

	using localconnectionObj::deserialized;

private:
	//! Normalize property name
	static std::string normalize(const std::string &name);


	class propsetresetaction;

	class getserverstatusdonecb;

	class resigndonecb;

	class newcertdonecb;

	//! Handle property sets and resets
	void dopropsetreset(propsetresetaction &action,
			    const std::string &name);

	//! Return this thread's status
	std::string report(std::ostream &rep);

	//! Allow gets with admin privileges

	bool allow_admin_get();

	class halted_cbObj;
};

#endif

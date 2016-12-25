/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef clusterlistenerimpl_H
#define clusterlistenerimpl_H

#undef libx_autogen_suppress_inner

#include "repoclusterinfoimplfwd.H"
#include "clusterlistenerimplfwd.H"
#include "stoppablethreadtrackerfwd.H"
#include "clusterlistener.H"
#include "spacemonitor.H"
#include "tobjrepofwd.H"
#include <x/property_value.H>
#include <x/fmtsize.H>
#include <x/semaphore.H>
#include <x/logger.H>
#include <x/threads/run.H>
#include "nsmap.H"
#include "stasher/namespace.H"

class trandistributorObj;
class clustertlsconnectshutdownObj;
class localconnectionObj;

//! Default implementation of clusterlistenerObj

//! This subclass provides default implementations of virtual methods in
//! clusterlistenerObj, and also implements a \c connectpeers message
//! that initiates connection attempts to unconnected peers.
//!
//! Invoke create_startarg() to create a starting argument for the thread.
//! The opaque object returned by create_startarg() should be used as the
//! thread starting argument.
//!
//! The cluster listener also monitors the amount of free space available
//! for objects. reservespace() allocates a x::df::reservation handle to reserve
//! some amount of space. outofspace() returns true if all reserved space
//! exceeds the available space left.
//!
//! Use calculate_alloc() to estimate the number of allocation units to reserve
//! for a file of a given size. Use calculate_inode() to estimate the number
//! of inodes to reserve for a file with the given filename (one, plus one for
//! each hierarchy delimiter character in the filename).

class clusterlistenerimplObj : public clusterlistenerObj {

	//! The logging object

	LOG_CLASS_SCOPE;

	//! An internal message to update reserved disk space reservation amount

	//! After the cluster configuration changes (added or removed peers),
	//! or other configuration changes, recompute the amount of space
	//! that should be reserved.

	class margin_message {

	public:
		//! Flag - the number of peers in the cluster has changed
		bool set_new_peers;

		//! How many peers there are in the cluster
		size_t npeers;

		//! Constructor -- something changed other than number of peers
		margin_message() : set_new_peers(false), npeers(0) {}

		//! Constructor -- number of peers has changed
		margin_message(size_t n) : set_new_peers(true), npeers(n) {}
	};

#include "clusterlistenerimpl.msgs.H"

private:
	//! Free space monitor
	spacemonitor spacedf;

	//! Semaphore for limiting the maximum number of objects being GET-ed

	x::semaphore getsemaphore;

	//! Maximum number of objects being GET-ed by all the clients

	static x::property::value<size_t> maxgetobjects;

public:
	//! Export space usage API, for regression test purposes

	spacemonitor df()
	{
		return spacedf;
	}

private:
	//! The "reserved::diskspace property"

	static x::property::value<x::memsize> reserved_space;

	//! The "reserved::inodes property"
	static x::property::value<size_t> reserved_inodes;

	//! Latest reported number of peers in the cluster
	size_t npeers;

	//! Reserved space kept as a margin

	//! Valid in the executing thread only

	spacemonitorObj::reservationptr *reservation_margin;

	class margin_callback;

	friend class margin_callback;
public:

	//! Constructor
	clusterlistenerimplObj(//! Object repository directory
			       const std::string &directoryArg);
	//! Destructor
	~clusterlistenerimplObj() noexcept;

private:
	class connectpeers_cb;

	//! Parameter to run(), on the executing thread's stack
	const STASHER_NAMESPACE::stoppableThreadTracker *tracker;

	//! Parameter to run(), on the executing thread's stack
	const x::ptr<trandistributorObj> *distributor;

	//! Parameter to run(), on the executing thread's stack
	const tobjrepo *distrepo;

	//! Parameter to run(), on the executing thread's stack
	const x::ptr<clustertlsconnectshutdownObj> *shutdown;

	//! Parameter to run(), on the executing thread's stack
	const repoclusterinfoimpl *cluster;

public:

	//! The thread run function
	void run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
		 const STASHER_NAMESPACE::stoppableThreadTracker  &trackerArg,
		 const x::ptr<trandistributorObj> &distributorArg,
		 const tobjrepo &distrepoArg,
		 const x::ptr<clustertlsconnectshutdownObj> &shutdownArg,
		 const repoclusterinfoimpl &clusterArg);

private:
	//! Implement start_network()

	//! Starts a clusterconnecterimplObj thread for this new network
	//! connection.
	void start_network(const x::fd &sock,
			   const x::sockaddr &addr);


	//! Implement start_privsock()

	void start_privsock(const x::fd &sock,
			    const x::sockaddr &addr);


	//! Implement start_pubsock()

	void start_pubsock(const x::fd &sock,
			   const x::sockaddr &addr);


	//! Receive local connection's userid, then start a connection


	void start_credentials(//! Local socket connection const x::fd &sock,
			       const x::fd &sock,

			       //! Start connection handler callback
			       void (clusterlistenerimplObj::*start_connArg)
			       (const x::fd &,
				const nsmap::clientcred &));


	class retr_credentialsObj;

	//! Common code for start_localconn() and start_privlocalconn()

	void start_conn(//! localprivconnectionObj is a subclass
			const x::ref<localconnectionObj> &conn,

			//! The file descriptor
			const x::fd &transport,

			//! The input iterator for the file descriptor
			const x::fd::base::inputiter &inputiter,
			//! The fdobjrwthreadObj mcguffin
			const x::ptr<x::obj> &mcguffin);

	//! Cluster's status has changed, make connection attempts to any new peers

	//! Simply calls connectpeers(), unless disabled by regression tests.

	void connectnewpeers();

	//! Disable connectnewpeers(), for regression tests.

	static x::property::value<bool> debugnonewconnect;

public:
	//! Directory with predefined application namespaces
	static x::property::value<std::string> appsdir;


	void foo() override {}
};

#endif

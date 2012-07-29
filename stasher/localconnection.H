/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef localconnection_H
#define localconnection_H

#undef libx_autogen_suppress_inner

#include "fdobjrwthread.H"
#include "writtenobj.H"
#include "stoppablethreadtracker.H"
#include "clusterstatusnotifier.H"
#include "clusternotifier.H"
#include "tobjrepo.H"
#include "stasher/userinit.H"
#include "stasher/userput.H"
#include "stasher/clusterstate.H"
#include "dirmsgreply.H"
#include "userputreply.H"
#include "usergetuuids.H"
#include "usergetuuidsreply.H"
#include "trandistributor.H"
#include "threadreportimpl.H"
#include "nsview.H"
#include "sendupdatesreq.H"
#include <x/semaphore.H>
#include <x/fmtsize.H>
#include "repoclusterinfoimplfwd.H"

#include "requestmsgs.auto.H"
#include "replymsgs.auto.H"

class spacemonitorObj;

//! A local client connection

class localconnectionObj : public STASHER_NAMESPACE::fdobjrwthreadObj,
			   public STASHER_NAMESPACE::threadreportimplObj,
			   public clusterstatusnotifierObj,
			   public clusternotifierObj {

#include "localconnection.msgs.decl.H"

protected:
	//! The logger

	LOG_CLASS_SCOPE;

	//! Convenience wrapper

	template<typename msg_type>
	class deser : public msg_type {

	public:
		//! Constructor

		template<typename obj_type>
		deser(obj_type &dummy) {}

		//! Destructor
		~deser() noexcept {}
	};

	//! This thread's name
	std::string name;

	//! Object repository
	tobjrepo repo;

	//! Namespace view on the repository
	nsview namespaceview;

	//! Tracker
	STASHER_NAMESPACE::stoppableThreadTracker tracker;

	//! Transaction distributor
	x::ptr<trandistributorObj> distributor;

	//! Object repository cluster metadata
	repoclusterinfoimpl cluster;

	//! Disk space monitor object
	spacemonitor spacedf;

	//! Semaphore for GET requests
	x::semaphore getsemaphore;

	//! Connection limits
	STASHER_NAMESPACE::userinit limits;

	//! Socket, on the executing thread's stack
	x::fd *socket;

	//! Current transaction being created, on the executing thread's stack
	x::ptr<STASHER_NAMESPACE::userput::deserializedObj> *newtran;

	class subscriptionsObj;

	//! Subscription object monitor, on the executing thread's stack

	x::ptr<subscriptionsObj> *subscriptions;

	//! Resolved pointer, to avoid having to dereference subscriptions, each time.

	subscriptionsObj *subscriptionsptr;

	class userchangedmsgObj;

	//! Property that sets maxgets
	static x::property::value<size_t> maxgets_prop;

	//! Property that sets the size of the write queue back to the client
	static x::property::value<size_t> writequeuesize_prop;

	//! Property that sets the maximum size of directory enumeration messages
	static x::property::value<size_t> diritersize_prop;

	//! Property that sets the maximum number of subscriptions
	static x::property::value<size_t> maxsubs_prop;
public:
	friend class STASHER_NAMESPACE::userput;
	friend class STASHER_NAMESPACE::puttransactionObj::content_str;

	template<typename iter_type> friend class deser;

	//! Deserialization classlist enumerator

	template<typename iter_type>
	static void classlist(iter_type &iter)
	{
		fdobjrwthreadObj::classlist(iter);

		iter.template serialize<STASHER_NAMESPACE::userput>();
		iter.template serialize<STASHER_NAMESPACE::puttransactionObj::content_str>();
		iter.template serialize<STASHER_NAMESPACE::usergetuuids,
					deser<STASHER_NAMESPACE::usergetuuids>
					>();
		iter.template serialize<STASHER_NAMESPACE::fdobjwriterthreadObj::sendfd_ready>();
		iter.template serialize<STASHER_NAMESPACE::sendupdatesreq,
					deser<STASHER_NAMESPACE::sendupdatesreq>
					>();
#include "localconnection.classlist.auto.H"
	}

	//! Constructor
	localconnectionObj(const std::string &threadnameArg,
			   const nsview &namespaceviewArg,
			   const tobjrepo &repoArg,
			   const STASHER_NAMESPACE::stoppableThreadTracker &trackerArg,
			   const x::ptr<trandistributorObj> &distributorArg,
			   const repoclusterinfoimpl &clusterArg,
			   const spacemonitor &spacemonitorArg,
			   const x::semaphore &getsemaphoreArg);


	//! Destructor
	~localconnectionObj() noexcept;

	//! The thread
	void run(//! The file descriptor
		 x::fd &transport,

		 //! The input iterator for the file descriptor
		 const x::fd::base::inputiter &inputiter,

		 //! Thread tracker that the writer thread gets registered with
		 const STASHER_NAMESPACE::stoppableThreadTracker &tracker,

		 //! The fdobjrwthreadObj mcguffin
		 const x::ptr<x::obj> &mcguffin);

	//! Start deserializing a transaction
	void deserialized(const STASHER_NAMESPACE::userput &msg);

	//! Deserializing a transaction
	void deserialized(const STASHER_NAMESPACE::puttransactionObj::content_str &msg);

	//! A request for contents of objects
	void deserialized(const STASHER_NAMESPACE::usergetuuids &msg);


	//! Deserializing a get
	void deserialized(const STASHER_NAMESPACE::fdobjwriterthreadObj::sendfd_ready
			  &msg);

	using fdobjrwthreadObj::deserialized;

	//! Deserialized a sendupdatesreq message

	void deserialized(const STASHER_NAMESPACE::sendupdatesreq &msg);


private:
	//! getuuids message rejected
	void usergetuuids_fail(const x::uuid &requuid,
			       const std::string &errmsg);


	//! Deserializing a transaction
	void accept_fds(//! How many file descriptors are coming
			size_t n);

	//! Deserializing a transaction

	void received_fd(const std::vector<x::fdptr> &fdArg);


protected:
	//! Return this thread's name

	std::string getName() const noexcept
	{
		return name;
	}
private:
	//! Check if the transaction has been deserialized
	void check_deserialized_transaction();

	//! The actual main loop

	MAINLOOP_DECL;

	//! The thread has started
	void started();

	//! Send a userputreply message back to the client
	void done(//! Request uuid
		  const x::uuid &uuidArg,

		  //! Transaction status
		  STASHER_NAMESPACE::req_stat_t status,

		  //! UUID of new objects
		  const x::uuid &newobjuuidArg);

	class put_cb;

	class senddir;

	//! Processing before main thread loop waits for I/O

	void drain();

	//! Pending GET request metadata

	class getinfo;

	//! Pending GET requests, on the executing thread's stack

	std::list<getinfo> *getqueue;

	//! Size of getqueue

	size_t getqueue_cnt;

	//! A GET request is ready to be processed

	void getready(const getinfo &gi, STASHER_NAMESPACE::usergetuuidsreply
		      &ack);


	class quorumstatusObj;

	//! Whether the cluster is currently in quorum, as far as we know.

	//! Valid in the context of the connection thread, only.
	//!
	STASHER_NAMESPACE::clusterstate currentstate;

	//! Whether the client wants updates
	bool wantupdates;

	//! Quorum's status has change, send update messages, if they are wanted

	void quorumstatuschanged();

	//! This connection object has received the current quorum status for the first time
	bool stat_quorum_received;

	//! This connection object has received the current cluster state for the first time
	bool stat_state_received;

	//! This connection object has received the current cluster configuration for the first time
	bool stat_cluster_received;

protected:
	//! Return this thread's status
	std::string report(std::ostream &rep);

public:
#include "localconnection.deserialized.auto.H"

private:

	//! Client cannot force admin privileges for a get request

	//! This gets overriden by a no-op by localprivconnectionObj.
	//!

	virtual bool allow_admin_get();

};

//! Minor specialization for deserializing a get request
template<>
class localconnectionObj::deser<STASHER_NAMESPACE::usergetuuids>
	: public STASHER_NAMESPACE::usergetuuids {

public:

	//! Constructor
	deser(localconnectionObj &conn)
		: usergetuuids(conn.limits.maxobjects) {}

	//! Destructor
	~deser() noexcept {}
};

#endif

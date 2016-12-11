/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef trandistributor_H
#define trandistributor_H

#undef libx_autogen_suppress_inner

#include <x/threadmsgdispatcher.H>
#include <x/threads/run.H>
#include <x/weaklist.H>
#include <x/logger.H>

#include "clusternotifier.H"
#include "tobjrepo.H"
#include "trandistuuid.H"
#include "newtran.H"
#include "transerializer.H"
#include "threadreportimpl2.H"
#include "repoclusterinfofwd.H"
#include "trandistreceivedfwd.H"
#include "repopeerconnectionfwd.H"
#include "stasher/reqstat.H"

class newtranObj;

class trandistihave;
class trandistcancel;

STASHER_NAMESPACE_START
template<typename msg_type> class writtenObj;
STASHER_NAMESPACE_END

//! A transaction distributor thread

//! The distributor is responsible for sending transactions originating from
//! this node to peer distributor objects running on other nodes in the cluster.
//! The distributor also keeps track of transactions received from its peer
//! distributor objects. The distributor forwards transaction uuids to a
//! receiver; this includes uuids of transactions originating from this node
//! or other nodes. Once a transaction is cancelled, its removed from the
//! object repository, and it no longer gets distributed to other peers.
//!
//! The transaction distributor is a critical thread.
//! create_startarg() returns an opaque object that should be used as a starting
//! argument for the thread.
//!
//! All transactions in the object repository are expected to have an
//! option "NODE" set to the name of the cluster node that's distributing
//! the transaction. [TRANDISTSETNODE]
//!
//! create_start() receives a reference to a clusterinfo object. The
//! distributor thread removes objects from the repository whose SOURCE is
//! not listed as node in the cluster. When started, the thread installs
//! itself as a cluster notifier, and each time the list of nodes in the
//! cluster changes, the transactions from removed nodes are purged from the
//! repository. The initial installation of the notifier callback triggers
//! this processing at thread startup [PURGETRANSOURCEUNKNOWN].
//!
//! \par Connecting with another distributor
//!
//! When this node establishes a connection with another peer, the
//! \c connected message
//! gets sent, specifying the peer's name, and the connection object.
//!
//! This distributor node enumerates transactions that this node already has
//! from the peer, and sends the peer a \c trandistihave message
//! [TRANDISTIHAVE].
//!
//! Upon receipt of a \c trandistihave message, the list of uuids the other
//! peer has from this node is compared with the list of finalized transaction
//! this distributor is distributing. All the uuids that the peer distributor
//! has, but this distributor no longer has, are sent in the \c trandistcancel
//! message, followed by a transerializer message for each uuid that the
//! this distributor is distributing, but the peer distributor does not have.
//! Afterwords, both this distributor and the peer distributor are in sync
//! [TRANDISTSYNC].
//!
//! \par Submitting a new transaction
//!
//! A new transaction gets submitted via newtransaction(), giving the handle
//! of the unfinalized transaction. The distributor thread finalizes the
//! transaction, and sends a transerializer message to all trandistsync-ed
//! distributor peers [TRANDISTNEW].
//!
//! newtransaction() returns a \c transtatus reference object, and holds
//! a reference on the specified mcguffin until the master controller
//! processes the transaction. The transaction status then gets placed into the
//! \c transtatus referenced object, and the reference on the mcguffin gets
//! released [TRANDISTDONE]. When the transaction is done, the uuid field
//! in the \c transtatus object contains the uuid of any new or updated objects
//! in the transaction [TRANDISTNEWUUID].
//!
//! If a transaction's stated SOURCE is not found in the list of all the nodes
//! in the cluster, the transaction is cancelled and never registered as a
//! received transactions [TRANSOURCEUNKNOWN].
//!
//! \par Cancelling a transaction
//!
//! A request to cancel a transaction originates from the distributing peer.
//! The distributing peer sends the
//! \c canceltransaction message after the master
//! controller commits or rejects the transaction. The distributor picks up
//! tobjrepoObj::done_hier/{source}/{uuid} object installation, as a signal
//! that the master controller processed the transaction.
//! The distributor threads cancels the transaction in
//! the repository, then sends a trandistcancel message to all trandistsync-ed
//! distributor peers [TRANDISTCANCEL].
//!
//! At startup, if there are existing tobjrepoObj::done_hier/{source}/{uuid}
//! objects, those transactions get cancelled immediately. This might happen
//! if the node was shut down just after the master sent a message to commit
//! or cancel the transaction, but the node went down after the object was
//! installed, but before the distributor thread picked up on it
//! [TRANDISTSTARTUPCLEANUP].
//!
//! \par Receivers
//!
//! The distributor objects notifies receivers of all transactions that
//! the distributor receives, from this node or other peer distributors.
//!
//! The \c installreceiver
//! message installs a receiver object. Upon installation, the
//! distributor invokes the receiver's received() callback, enumerating all
//! transactions that the distributor has received, and not cancelled, prior
//! to the receiver's installation [TRANDISTRECVINSTALL].
//!
//! Afterwards, the distributor invokes the receiver's received() and
//! cancelled() callbacks, specifying new transactions that the distributor
//! has received [TRANDISTRECVADD], and existing transactions that the
//! distributor has cancelled [TRANDISTRECVREMOVE].
//!
//! When the \c installreceiver message
//! installs a receiver object, any previous receiver
//! object gets automatically deinstalled, and it will no longer have its
//! callback objects invoked. The distributor notifies only one receiver object
//! at a time, each installed receiver supercedes the previous one.
//!
//! \par Failed transactions
//!
//! A peer connection object may report a failure in receiving a transaction
//! by sending a \c deserialized_fail. This typically happens when there is no
//! sufficient space to store the transaction. The transaction ID is saved
//! internally in the repository object, and is reported to the receiver, with
//! a failed status [TRANFAILEDANNOUNCERECV].
//!
//! If the sending peer disconnects and reconnects, the transaction is
//! reported to the peer as if it was received [TRANFAILEDIHAVE].
//!
//! If the sending node is removed, the failed transaction is removed, and
//! reported to the receiver as being cancelled [TRANFAILEDREMOVERECV].

class trandistributorObj : virtual public x::threadmsgdispatcherObj,
			   public x::runthreadname,
			   public clusternotifierObj,
			   public STASHER_NAMESPACE::threadreportimpl2Obj {

	//! The logger object
	LOG_CLASS_SCOPE;

	//! The repoclusterinfo object, in the context of the execution thread
	repoclusterinfoObj *cluster;

	//! The transaction repo, in the context of the execution thread
	tobjrepoObj *repo;

	//! Current transaction receiver object, in the context of the thread
	trandistreceivedObj *receiver;

	//! Reference to the current transaction received object, in the thread
	trandistreceivedptr *receiverref;

	//! A container for distributor peers
	typedef x::weaklist<repopeerconnectionObj>peer_list_t;

	//! Distributor peers

	peer_list_t peer_list;

	//! My node name, valid in the context of the executing thread
	std::string nodename;

	//! Return the thread's name

	std::string getName() const;
public:
	//! The status of the transaction, after its been processed.

	//! A reference to this object gets returned by newtransaction().
	//!

	class transtatusObj : virtual public x::obj {

	public:
		//! Constructor
		transtatusObj();

		//! Destructor
		~transtatusObj() noexcept;

		//! Transaction status
		STASHER_NAMESPACE::req_stat_t status;

		//! UUID of new and updated objects
		x::uuid uuid;
	};

	//! A reference returned by newtransaction()

	typedef x::ptr<transtatusObj> transtatus;

private:

	class trandistinfo_t;

	//! The container for statuses of transactions we're distributing

	typedef std::map<x::uuid, trandistinfo_t> trandistlist_t;

	//! Transactions we're distributing, on the executing thread's stack

	trandistlist_t *trandistlist;

	//! Load transaction list upon startup

	void load_trandistlist();

	//! Create a serialization message.

	static x::ref<STASHER_NAMESPACE::writtenObj<transerializer > >
	create_serialization_msg(const tobjrepo &repo,
				 const x::uuid &uuid);

public:

	void clusterupdated(const clusterinfoObj::cluster_t &status_arg)
		override;
private:
	//! Most recent list of all nodes in the cluster

	//! Accessed by the executing thread, only.

	std::set<std::string> known_nodes;
public:

	class startarg;

	//! Constructor
	trandistributorObj();

	//! Destructor
	~trandistributorObj() noexcept;

	void foo() override {}

	//! The execution thread

	void run(//! start_thread()'s mcguffin
		 x::ptr<x::obj> &threadmsgdispatcher_mcguffin,

		 //! My message queue, constructed in advance.

		 const msgqueue_obj &msgqueue,

		 //! The repoclusterinfo object.
		 const repoclusterinfo &cluster,

		 //! The repository
		 const tobjrepo &repo,
		 //! The thread mcguffin.
		 //! The thread keeps the mcguffin on its execution
		 //! stack, and releases the reference upon termination.
		 const x::ptr<x::obj> &mcguffin);

private:
	class purge_cb;

protected:
	//! Implement dispatch of cluster update, overridden by a subclass in a regression test

	virtual void do_dispatch_cluster_update(const clusterinfoObj::cluster_t
						&newStatus);


public:

	//! Submit a new transaction for distribution

	//! The distributor holds a reference on the mcguffin while the
	//! transaction is begin processed. If no other references remain,
	//! once the mcguffin gets destroyed, the transtatus->status indicates
	//! what happened to the transaction.

	transtatus newtransaction(//! Unfinalized transaction
				  const newtran &newtran,

				  //! Mcguffin
				  const x::ptr<x::obj> &mcguffin);

	//! An internal transaction to be processed

	//! \see internal_transaction()
	//!

	class internalTransactionObj : virtual public x::obj {

	public:
		internalTransactionObj() {}
		~internalTransactionObj() noexcept {}

		virtual void doit()=0;
	};

	//! Process a transaction internally

	//! This is used by the cluster connecter in a new cluster node upon
	//! receiving its first connection from the peer, in order to install
	//! the cluster peer list. The peer list need to be installed using
	//! a transaction, but outside of the normal distribution logic.
	//!
	//! Invokes ->doit() in the context of the distributor thread, with
	//! no further action.

	virtual void internal_transaction(const x::ref<internalTransactionObj>
					  &tran);


	//! Received a trandistihave message
	void deserialized(//! The message
			  const trandistihave &msg,

			  //! The connection that sent the message
			  const repopeerconnectionptr &connection);

	//! Received a trandistcancel message
	void deserialized(//! The message
			  const trandistcancel &msg);

	//! Deserialized a message
	void deserialized(//! The message
			  const transerializer &msg);

private:

	class enumerate_ihave;

	//! Process a request to cancel a transaction with the given uuid.
	void dispatch_cancel(const x::uuid &uuid);


	class received_transactions_cb;

	//! Received a new transaction, originated here or from another node.

	//! This received() finalizes the transaction, then proceeds with the
	//! new transaction processing.
	x::uuid process_received(const newtran &newtran);


	//! Process a received transaction

	//! The finalized transaction notifies the receiver. If the transaction
	//! failed, its added to the internal list of failed transactions.
	//! In either case, the receiver gets notified.

	void process_received(const x::uuid &uuid,
			      const dist_received_status_t &status);


	//! Cancelling an existing transaction.

	//! The transaction was originated here or was distributed by another
	//! node. The transaction is cancelled, and the receiver is notified.

	void cancelled(const x::uuid &uuid);

	//! Internal run loop.

	void runloop(msgqueue_auto &msgqueue);

	//! Report on the distributor's status

	std::string report(std::ostream &rep) override;

#include "trandistributor.msgs.H"

protected:
	void do_dispatch_deserialized_cancel(const trandistcancel &cancel) ;
	void do_dispatch_deserialized_transaction(const newtran &tran,
						  const x::uuid &uuid) ;
};

#endif

/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef repocontrollermaster_H
#define repocontrollermaster_H

#include "repocontrollermasterfwd.H"
#include "repocontrollerbasehandoff.H"
#include "repopeerconnectionbase.H"
#include "batonfwd.H"
#include "clusterinfo.H"
#include "threadreportimpl.H"
#include "objrepocopydstinterfacefwd.H"
#include "objrepocopysrcinterfacefwd.H"
#include "repopeerconnectionfwd.H"
#include "tranuuidfwd.H"
#include "tranreceivedfwd.H"
#include "trandistuuidfwd.H"
#include "trandistreceivedfwd.H"
#include "stasher/quorumstate.H"
#include <x/threads/run.H>
#include <x/threads/workerpool.H>
#include <x/stoppable.H>
#include <x/weakptr.H>
#include <map>

class clusterinfoObj;
class objrepocopydstinterfaceObj;
class trandistributorObj;
class trancancelleduuidObj;

//! Master repository controller

//! After constructing this object, initialize() must be invoked to install
//! a callback that synchronizes the controller's list of peers in the cluster
//! with what the repository shows. The callback gets installed as a cluster
//! notifier. The callback sends the clusterupdated message to the object.
//!
//! The master controller ignores status updates from peers that are not
//! found in the list [MASTERREJECTPEER].
//!
//! Status updates from peers are forwarded to the repository controller.
//! If the peer reports that this node is its master, the controller sends the
//! node a peerlink message and a mastersyncinfo message, if they have not been
//! sent already [MASTERPEERLINK].
//!
//! When the peerlink message gets destroyed, the peer declares that it's
//! no longer a slave, so it's dropped from the list of slaves [PEERNOTSLAVE].
//!
//! Peer connection objects send \c transactions_received and
//! \c transactions_cancelled messages
//! to report when they receive transactions from their
//! peers (and cancel them, upon the distributing peer's request). When
//! connected to a slave peer, the master controller creates a tranuuidObj
//! structure for that peer that holds transactions that get received by the
//! peer, and the connection object sends \c transactions_received and
//! \c transactions_cancelled passing the object as the first argument, and the
//! list of transaction uuids as the second argument. These two messages
//! have the actual list updated in the context of master
//! controller's thread. Afterwards, for \c transactions_received, the master
//! checks if all peers have now received the transactions (together with
//! this node), and if so, the transactions gets scheduled for committing.
//!
//! After a peer is connected or disconnected, or the cluster configuration
//! changes, all pending transactions are rechecked to see if they can be
//! commited.
//!
//! Once a transaction has been verified as been distributed to all threads,
//! it gets dispatched to a commit thread.
//!
//! Before the transaction gets dispatched to the commit thread, a check gets
//! made if any node, including this node, reported a failure in receiving
//! the transaction.
//!
//! The commit threads verifies the objects in the transactions. If a node
//! reported a failure in receiving the transaction, the transaction gets
//! rejected with a "failed" status [COMMITFAILONRCPT].
//! If all nodes received the transactions,
//! but any object in the transaction referencing an existing object does not
//! supply the correct uuid, the transaction gets rejected with a "rejected"
//! status. If all uuids match the transaction gets commited.
//! First, commit_peer() gets invoked for all peers except the one that's
//! distributing the transaction. Then, the transaction gets commited on the
//! master, then commit_peer() gets invoked for the distributing peer
//! A rejected transaction is also announced in the same order.
//! [COMMITORDER].
//!
//! \par Handing off master status
//!
//! handoff_request() takes the name of another peer in the cluster.
//! First, the commits get blocked [MASTERHANDOFFBLOCK] and a commit lock
//! on the repository gets acquired.
//!
//! If the handoff process fails to complete, the commits get reestablished
//! [MASTERHANDOFFABORT].
//!
//! A baton announce message gets sent to all peer connection threads
//! except the one that will become the new master [MASTERHANDOFFANNOUNCE].
//! A mcguffin gets attached
//! to the request message, which the peer connection thread releases
//! once the announce message has been processed.
//!
//! handoff_request() returns a mcguffin, and releases its reference on the
//! mcguffin when
//! the cluster has a new master. The new master may not necessarily have a
//! full quorum, and may still be in the process of resyncing with some of its
//! slaves.

class repocontrollermasterObj : public repocontrollerbasehandoffObj,
				public STASHER_NAMESPACE::threadreportimplObj {

	//! The logger

	LOG_CLASS_SCOPE;

	class master_mcguffin;

	class clusterNotifierCallbackObj;

	//! Cluster notification callback

	x::ptr<clusterNotifierCallbackObj> clusterNotifierCallback;

	//! Our quorum state shalt be this

	STASHER_NAMESPACE::quorumstate curquorum;

	//! The distributor -- may be NULL for some regression tests
	x::weakptr<x::ptr<trandistributorObj> > distributor;

	//! Invoke stop() after halting the entire cluster
	x::stoppable haltstop;

public:
	//! Initialize the notifier callback

	void initialize(const clusterinfo &cluster);

	//! Master's mcguffin reference to the slave

	class slaveConnectionObj;

	//! A reference to a slave connection object

	typedef x::ref<slaveConnectionObj> slaveConnection;

	//! A pointer to a slave connection object

	typedef x::ptr<slaveConnectionObj> slaveConnectionptr;

private:
	//! Metadata that's kept for each slave

	class slaveinfo;

	//! Container for slave metadata

	typedef std::map<std::string, slaveinfo> slaves_t;

	//! The slaves -- on the executing thread's stack

	slaves_t *slaves;

	class thisnodereceivedObj;

	//! Whether the controller thread received the initial cluster state notification message
	bool received_cluster_update;

	//! Transactions that have been received by this node -- on the executing thread's stack
	thisnodereceivedObj *thisnodereceived;

	//! A mcguffin on the execution thread's stack

	x::ptr<x::obj> *controller_mcguffin;

	class commitThreadObj;

	//! An internal commit thread

	typedef x::ref<commitThreadObj> commitThread;

	//! The type of a worker pool of threads that commit transactions.

	typedef x::workerpool<commitThreadObj> worker_pool_t;

	//! Threads that commit transactions, on the execution thread's stack.
	worker_pool_t::obj_type *commitqueue;

	//! A mcguffin, on the stack, that gets destroyed before commitqueue

	//! This is used as a mcguffin to terminate pending commits.

	x::weakptr<x::ptr<x::obj> > *stop_mcguffin;

	//! The repository cluster, on the executing thread's stack

	clusterinfoptr *cluster;

public:
	friend class clusterNotifierCallbackObj;
	friend class batonObj;

	//! Constructor
	repocontrollermasterObj(//! Master node name, either this node or another on in the cluster.
				const std::string &masternameArg,

				//! Master instance uuid
				const x::uuid &masteruuidArg,

				//! The object repository
				const tobjrepo &repoArg,

				//! Quorum callback notification list
				const repoclusterquorum &callback_listArg,

				//! The distributor object

				//! Some regression tests may pass a NULL
				//! reference, which disables linkage with the
				//! distributor.
				const x::ptr<trandistributorObj>
				&distributorArg,

				//! The thread tracker that starts this thread
				const STASHER_NAMESPACE::stoppableThreadTracker &trackerArg,

				//! Invoke stop() after halting the entire cluster
				const x::stoppable &haltstopArg);


	//! Destructor
	~repocontrollermasterObj();

	//! Callback helper for binding source interface when syncinc a slave

	//! bind() gets invoked when syncslave() is processed and a repository
	//! copy thread gets started. This callback receives the source
	//! interface, which the peer connection object uses to connect to
	//! its source interface.
	class syncslave_cbObj: virtual public x::obj {

	public:
		//! Constructor
		syncslave_cbObj();
		//! Destructor
		~syncslave_cbObj();

		//! Bind to this source interface
		virtual void bind(const objrepocopysrcinterfaceptr &src)=0;
	};

	//! Retrieve the current connection with some peer, for debugging purposes

	repopeerconnectionptr
	debugGetPeerConnection(const std::string &peername);

	//! Invoke haltstop->stop(), for debugging purposes

	void debugHalt()
	{
		haltstop->stop();
	}

	class halt_continue_cbObj;
	class halt_cbObj;

	//! Start this thread

	start_controller_ret_t start_controller(const msgqueue_obj &msgqueue,
						const x::ref<x::obj> &mcguffin)
		override;

	//! The thread

	void run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
		 const msgqueue_obj &msgqueue,
		 x::ref<x::obj> &start_arg);

	void foo() override {}
private:
	//! Verify whether quorum is present.

	void compute_quorum(bool forceannounce);

	//! An object that lists all the peers in the quorum

	class quorumPeerListObj;

	//! A list of all the peers currently in the quorum

	typedef x::ref<quorumPeerListObj> quorumPeerList;

	//! Verify whether quorum is present, and retrieve the list of all present peers.

	bool compute_quorum_get_peers(bool forceannounce,
				      const quorumPeerList &peers);


	class debug_get_peer_msg;

	//! Check for completed repository copy threads

	void check_copy_completed();

	//! Check if some transactions are ready to be commited

	void checkcommit(const trandistuuid &uuids);


	//! Check if some transactions may be commited to a quorum

	void checkcommit(const trandistuuid &uuids,
			 const quorumPeerList &peers);


	class commitJobObj;

	//! Commit lock mcguffin

	//! Accessed only by the executing thread.
	//! This mcguffin gets placed in every commit job. A null
	//! mcguffin blocks commit jobs from getting started.
	//!
	//! To halt all commits, as part of transfering master authority to
	//! another node, the existing mcguffin is removed, and a destructor
	//! callback gets attached that invokes handoff_initiate() when
	//! the mcguffin gets destroyed.
	//!
	//! If the final handoff does not happen, a new mcguffin gets created,
	//! and everything gets kick-started into motion.
	//!
	x::ptr<x::obj> commit_mcguffin;

public:
	//! Request the master to hand itself off to some other peer

	x::ptr<x::obj>
	handoff_request(const std::string &peername) override;

	class handoff_msgObj;

private:

	class handoff_destroy_cb;

	class handoff_repolockthreadObj;

	class announce_mcguffin;

	//! Report this thread's status

	std::string report(std::ostream &rep) override;

#include "repocontrollermaster.msgs.H"

public:
	//! Wrapper for set_cluster().
	void set_cluster(//! Message parameter
			 const x::weakptr<clusterinfoptr> &cluster) ;
	//! Wrapper for accept().
	void accept(//! Message parameter
		    const repopeerconnectionptr &peer,
		    //! Message parameter
		    const slaveConnectionptr &conn,
		    //! Message parameter
		    const repopeerconnectionbaseObj::peerlinkptr &link);

	//! Wrapper for peernewmaster().
	void peernewmaster(//! Message parameter
			   const repopeerconnectionptr &peerRef,
			   //! Message parameter
			   const nodeclusterstatus &peerStatus) override;

	//! Wrapper for syncslave();
	void syncslave(//! Message parameter
		       const x::uuid &connuuid,
		       //! Message parameter
		       const x::weakptr<objrepocopydstinterfaceptr> &peer,
		       //! Message parameter
		       const std::string &name,
		       //! Message parameter
		       const batonptr &newmasterbaton,
		       //! Message parameter
		       const x::ptr<syncslave_cbObj> &cb);

	//! Wrapper for get_quorum()
	void get_quorum(//! Message parameter
			const STASHER_NAMESPACE::quorumstateref &status,
			//! Message parameter
			const boolref &processed,
			//! Message parameter
			const x::ptr<x::obj> &mcguffin) override;

	//! Wrapper for halt()

	void halt(//! Result of the request, to be determined
		  const STASHER_NAMESPACE::haltrequestresults &req,

		  //! Mcguffin.
		  const x::ref<x::obj> &mcguffin) override;
};


#endif

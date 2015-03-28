/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef clusterinfo_H
#define clusterinfo_H

#include "nodeinfoconn.H"
#include "nodeclusterstatus.H"
#include "clusternotifierfwd.H"
#include "clusterstatusnotifierfwd.H"
#include "clusterinfofwd.H"
#include "stoppablethreadtrackerfwd.H"
#include "batonfwd.H"
#include <x/weakmap.H>
#include <x/rwlock.H>
#include <x/vipobj.H>
#include <x/vipobjdebug.H>
#include <map>

#include "stasher/namespace.H"

class peerstatusObj;

//! Cluster information data

//! The constructor takes a name that becomes the permanent name of this
//! node in the cluster.
//!
//! \par Updating the list of other nodes in the cluster
//!
//! update() takes an updated list of nodes in the cluster. If a node with
//! a given name no longer appears in the updated list, if there's an
//! installed connection for that node, its stop() method gets
//! invoked. [STOP]
//!
//! If the updated list contains a different list of nodes than
//! the current list, or changes the contents of an entry for a node,
//! update() invokes recalculate(). [CLUSTERUPDATE]
//!
//! If the list contains an entry for this node's name, this node's
//! information is updated with the new information in the entry.
//!
//! installnotifycluster() installs a callback for changes to the list of
//! nodes. The callback object's clusterupdated() gets immediately invoked
//! with the current list of nodes in the cluster, then it gets invoked
//! each time the list gets changed by update() [UPDATECALLBACK].
//!
//! The clusterupdated() callback also gets invoked when a new connection
//! has been established with some peer, or when an existing connection
//! goes away [UPDATECALLBACK2].
//!
//! \par Registering connections
//!
//! installpeer() takes the name of the node and a reference to its peerstatus
//! object and attempts to register it with this cluster object.
//! installpeer() returns true if the registration succeeds, and the
//! connection object's thread should be started.
//!
//! installpeer() returns a bool+peerstatus tuple.
//! Registration fails, and \c false gets returned in the bool part of the
//! tuple, with the second part being an empty reference,
//! if no such peer with the given name exists.
//! Registration fails, and \c false gets returned in the bool part of the
//! tuple, and a reference to an existing peerstatus object in the second
//! part of the tuple if the peer exists
//! but already has a registered connection. [FAILED]
//!
//! Otherwise, installpeer() invokes the peerstatus object's initialstatus()
//! method with this node's current status, then registers this connection
//! as the connection for this node, as a weak reference, simultaneously
//! registers it as a status update notifier, then it invokes recalculate(),
//! and returns true in the bool part of the tuple, with the peerstatus
//! part being an empty reference. [REGISTERED]
//!
//! Given a name of a peer, getnodepeer returns a reference to a peerstatus
//! object that's registered for this peer, or a null reference if it's
//! not registered [GETNODEPEER].
//!
//! The peerstatus object gets registered as if via installnotifyclusterstatus(), except
//! that initialstatus() gets invoked instead of statusupdated(). However,
//! if recalculate() ends up updating the status of this node, as a result of
//! the newly registered connection, statusupdated() will get called immediately
//! after the peerstatus object's initialstatus() returns.
//!
//! The peerstatus object must be already initialized with the peer's initial
//! status, before installpeer() gets invoked.
//!
//! pingallpeers() invokes ping() for each registered connection, using the
//! same mcguffin [PINGALLPEERS].
//!
//! \par Outgoing connections.
//!
//! connectpeers() makes outgoing connections to peers that do not have
//! an established connection.
//!
//! The actualc onnection attempt implementation gets defined elsewhere.
//! connectpeers() merely identifies peers that:
//!
//! - do not have an established connection [CONNECTPEERS]
//!
//! - do not already have a connection attempt in progress initiated by the
//! previous call to connectpeers() [RECONNECTPEERS]
//!
//! For each such peer, connectpeers() invokes the callback functor which,
//! presumably creates an object that takes care of the connection attempt.
//! Another connection attempt does not get made until the object returned
//! by the callback method gets destroyed, even if a connection with the
//! peer is still not registered.
//!
//! \note
//! This clusterinfo object remains locked for the duration of this
//! method, so the callback cannot generally call any methods
//! in this clusterinfo object.
//!
//! The caller should instantiate a clusterconnecter, and save it in
//! an internal list, and when connectpeers() returns, proceed and
//! start the threads.
//!
//! \par Register status update notifiers
//!
//! The installnotifyclusterstatus() method takes a clusterstatusnotifier reference, and
//! adds it to the list of callback for changes to the status of this cluster
//! node, then invokes the reference's statusupdated() method with this node's
//! current status. [FIRSTSTATUS]
//!
//! \par Transferring a baton
//!
//! installbaton() gets invoked to store a "baton" object, indicating that
//! the current master in this cluster is handing over the master status to
//! another peer. Both the old master and the new master may or may not be
//! this node. The current master must be the old master, as given by the
//! baton, otherwise the installation gets cancelled.
//! The node's status gets recalculated [INSTALLBATON].
//!
//! clusterinfoObj holds a weak reference on the baton object. installbaton()
//! attaches a destructor callback that recalculates the node's status when
//! the baton object goes out of scope [UNINSTALLBATON].
//!
//! getbaton() returns a reference to the installed baton.
//!
//! The presence of the installed baton is a factor in the node status
//! calculation.
//!
//! \par Node status calculations
//!
//! calculate() calculates the status of this node as follows.
//!
//! - If there's an installed baton, if the name of the baton's resigning master
//! is not this node or some other node in the cluster, or if the name of the
//! baton's new master is not this node or some other node in the cluster
//! the baton gets removed [BATONCANCELUNKNOWNNODE].
//!
//! - If there's an installed baton indicating that new master is another peer
//! in the cluster, and there's no registered connection to the peer, the
//! baton gets removed [BATONNEWMASTERGONE].
//!
//! - If there's an installed baton, and the replacement node is this node's
//! name, and this node's status is not currently a master, change this node's
//! status to indicate that it's a master [BATONMASTERISME].
//!
//! - If there's an installed baton, and the current status indicates that
//! the master is not the new master, the current status is set to the new
//! master given in the baton, unless the old master is this node [BATONFORCE].
//!
//! - If the current status is a slave to some other master, there must be
//! a registered connection to that peer, and that peer must give its status
//! as the master. Otherwise, the node's status is reset to be its own master,
//! and it's UUID gets reset. [MASTERGONE]
//!
//! - If this node is a slave to a master node, this node's status is updated
//! with the master's status. [MASTERUPDATE]
//!
//! - If this node is a master, this node's slave count is updated to reflect
//! the number of all connected nodes that identify themselves as a slave to
//! this node, and which repost the same UUID as this master's UUID.
//! [COUNTSLAVES]
//!
//! - The status of all other connected nodes gets checked, except for nodes
//! that identify themselves as slaves to this node. If a connected node's
//! status is greater than the current status, this node's status is updated
//! to the new status [BETTERMASTER]. This step is skipped if there's an
//! installed baton [BATONNOBETTERMASTER].
//!
//! If calculate() calculates a status that's different than the current status,
//! all registered notifiers get invoked with the new status. [NOTIFYSTATUS]
//!
//! \par Getting node information
//!
//! getnodeinfo() retrieves the entry for the requested node. An exception
//! gets thrown if the specified node's name does not exist. [GETNODEINFO]
//!
//! getthisnodeinfo() retrieves the entry for this node. [GETTHISNODE]

class clusterinfoObj : virtual public x::obj {

protected:
	//! This node's name
	std::string nodename;

	//! This cluster's name
	std::string clustername;

	//! Thread tracker

	STASHER_NAMESPACE::stoppableThreadTracker thread_tracker;

private:
	//! This node's info
	STASHER_NAMESPACE::nodeinfo thisnode;

	//! This node's status is a Very Important Object

	typedef x::vipobj<nodeclusterstatus> thisnodestatus_t;

	//! Calculated status of this node
	thisnodestatus_t thisnodestatus;

	//! Baton being passed to another master in this cluster
	x::weakptr<batonptr > batonp;

public:
	//! Container for other peers in the cluster
	typedef std::map<std::string, nodeinfoconn> cluster_t;

private:
	//! cluster_t is a Very Important Object
	typedef x::vipobj<cluster_t> vipcluster_t;

	class destroycb;

	//! Other peers in the cluster
	vipcluster_t cluster;

public:
	//! Return true if there are no other peers in the cluster

	//! This might indicate a newly added node in the cluster, that has
	//! not been fully configured. A first connection from the peer will
	//! transfer the list of nodes into this newly-added node.

	bool empty()
	{
		return vipcluster_t::readlock(cluster)->empty();
	}
private:
	//! A container for existing connection attempts to other peers
	typedef x::weakmap<std::string, x::obj> connection_attempts_t;

	//! Existing connection attempts
	connection_attempts_t connection_attempts;

public:
	friend class peerstatusObj;

	//! Constructor
	clusterinfoObj(const std::string &nodename,
		       const std::string &clustername,
		       const STASHER_NAMESPACE::stoppableThreadTracker
		       &thread_trackerArg,
		       const STASHER_NAMESPACE::nodeinfomap &clusterinfo);


	//! Destructor
	~clusterinfoObj() noexcept;

	//! Update list of nodes in the cluster

	void update(const STASHER_NAMESPACE::nodeinfomap &newclusterinfo);


	//! Retrieve the list of nodes in the cluster (including this node)
	void get(STASHER_NAMESPACE::nodeinfomap &clusterinfo);


	//! Install a baton, and recalculate the node status

	bool installbaton(const baton &batonArg);

	//! Remove the baton
	void clearbaton(const baton &batonArg);

	//! Get the baton
	batonptr getbaton();

	//! Install a new peer connection
	std::pair<bool, x::ptr<peerstatusObj> >
	installpeer(const std::string &nodename,
		    const x::ptr<peerstatusObj> &node);


protected:

	//! Tail end of installpeer()

	//! The peer's name is good, and there is no existing connection.
	//! Save the peer, invoke the new peer's initialstatus() method,
	//! add it to the status_notifier_list, invoke recalculate_locked().

	virtual std::pair<bool, x::ptr<peerstatusObj> >
	installpeer_locked(const std::string &nodename,
			   const x::ptr<peerstatusObj> &node);


public:
	//! Callback for connectpeers()

	//! connectpeers() invokes this operator() to create a thread for a
	//! connection attempt to the given peer.

	class connectpeers_callback {

	public:
		//! Create a connection thread

		//! \return a reference to an opaque object that represents
		//! a connection attempt to the given peer. connectpeers() will
		//! not try the same peer again the next time it's called if
		//! this object still exists.

		virtual x::ptr<x::obj> operator()(const std::string &peername)
			const=0;
	};

	//! Make connection attempts to all unconnected peers.

	void connectpeers(const connectpeers_callback &callback);


	//! Install a cluster notification object

	void installnotifycluster(const clusternotifier &notifier);


	//! Install a cluster status notification object
	void installnotifyclusterstatus(const
					clusterstatusnotifier
					&notifier);


	//! A peer's status has changed

	//! The default implementation in this class invokes recalculate().
	//!

	virtual void peerstatusupdated(//! The peer whose status has changed
				       const x::ptr<peerstatusObj> &peerRef,

				       //! Its new status
				       const nodeclusterstatus &peerStatus);


	//! A peer announced that the peer has a new master

	//! The default implementation is a no-op, this is overriden in
	//! the subclass.
	virtual void peernewmaster(//! The peer whose master has changed
				   const x::ptr<peerstatusObj> &peerRef,

				   //! Its new status
				   const nodeclusterstatus &peerStatus);


	//! Recalculate this node's status.
	virtual void recalculate();

	//! Return entry for a node

	void getnodeinfo(const std::string &nodename,
			 STASHER_NAMESPACE::nodeinfo &infoRet);

	//! Return the connection for a node, if it exists.

	x::ptr<peerstatusObj> getnodepeer(const std::string &nodename);

private:
	//! Return the connection for a node, if it exists, with a lock

	x::ptr<peerstatusObj>
	getnodepeer_locked(const vipcluster_t::readlock &clusterlock,
			   const std::string &nodename);

public:
	//! Return all node connections

	void getallpeers(//! All connections are added to this list
			 std::list<x::ptr<peerstatusObj> > &allpeerList);


	//! Invoke ping() for all node connections, with the same mcguffin

	void pingallpeers(//! mcguffin to use for all pings

			  const x::ptr<x::obj> &mcguffin);

	//! Invoke installformermasterbaton() for all node connections

	void installformermasterbaton(const baton &batonp);


	//! Return this node's entry

	void getthisnodeinfo(STASHER_NAMESPACE::nodeinfo &infoRet);

	//! Return this node's name

	const std::string &getthisnodename() const noexcept
	{
		return nodename;
	}

	//! Return this cluster's name

	const std::string &getthisclustername() const noexcept
	{
		return clustername;
	}

	class status;

	friend class status;

protected:


	//! When the new status of this node gets calculated, keep track of who is this node's master.

	//! If this node is a slave to a master, specify who the master is.
	//!

	class newnodeclusterstatus {

	public:
		//! Calculated node status
		nodeclusterstatus status;

		//! If this node is a slave, this is the master
		x::ptr<peerstatusObj> masterpeer;

		//! Constructor
		newnodeclusterstatus(const nodeclusterstatus &statusArg);


		//! Destructor
		~newnodeclusterstatus() noexcept;
	};

	//! Recalculate the status, with this object being locked.

	//! If the status is different, invoke notifystatuschange_locked()
	//!
	void recalculate_locked(thisnodestatus_t::updatelock &lock);


	//! Calculate a new status
	newnodeclusterstatus calculate_status_locked(const vipcluster_t
						     ::readlock &);


	//! Calculate a new status with an installed baton

	nodeclusterstatus calculate_status_locked_with_baton(const vipcluster_t
							     ::readlock &);


	//! Node status changed, and it's a new master

	//! This gets invoked when the node's status changes to a different
	//! master. This method is subclassed in repoclusterinfoObj, which
	//! start a new controller.

	virtual void startmaster(const newnodeclusterstatus &newStatus);

private:
	//! Update cluster metadata

	//! Updates thiscluster and status.

	void do_update(//! New cluster information
		       const STASHER_NAMESPACE::nodeinfomap
		       &newclusterinfo,

		       //! Whether to invoke recalculate() after updating.

		       //! This is given as \c false when do_update() gets
		       //! invoked by the constructor, and \c true
		       //! when update() gets invoked.
		       bool do_recalculate);


	//! Invoked by do_update() after locking the notifier list

	void do_update_locked(const STASHER_NAMESPACE::nodeinfomap
			      &newclusterinfo,
			      bool do_recalculate,
			      bool &recalculate_needed);


public:
	//! Report on the cluster's status

	void report(std::ostream &o);

	//! Used by regression tests

	//! \internal
	void debugUpdateTimestamp();

};

//! This node's status

//! Instantiate the status object to retrieve the node's current status.
//! This object acts like a pointer to a nodeclusterstatus,
//! and defines the -> and * operators that access the object.
//! This is a thread-safe operation with respect to
//! peerstatusObj::statusupdate().

class clusterinfoObj::status {

	//! My connection node
	clusterinfoptr info;

	//! Native pointer.

	clusterinfoObj *ptr;

	//! A lock on the very important thisnodestatus object

	thisnodestatus_t::readlock lock;

private:
	//! Internal constructor for clusterinfoObj members only

	status(clusterinfoObj *p)
		: ptr(p), lock(p->thisnodestatus) {}

public:
	friend class clusterinfoObj;

	//! Constructor
	status(const clusterinfo &infoArg)
		: info(infoArg), ptr(&*info), lock(ptr->thisnodestatus) {}

	//! Destructor
	~status() noexcept {}

	//! Dereference operator
	const nodeclusterstatus *operator->() const
	{
		return &*lock;
	}

	//! Dereference operator
	const nodeclusterstatus &operator*() const
	{
		return *lock;
	}
};

#endif



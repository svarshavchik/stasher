/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef repoclusterinfo_H
#define repoclusterinfo_H

#include "clusterinfo.H"
#include "nodeclusterstatus.H"
#include "repoclusterquorum.H"
#include "repocontrollerbasefwd.H"
#include "stasher/repoclusterquorumcallbackbase.H"
#include "stasher/quorumstateobj.H"
#include "repoclusterinfofwd.H"
#include "boolreffwd.H"
#include "tobjrepo.H"
#include <x/logger.H>

class repocontrollerbaseObj;
class peerstatusObj;

//! Object repository cluster manager

//! The subclass must inherit from x::stoppableObj, in addition to implementing
//! the pure virtual methods.
//!
//! The constructor takes the name of this node in a cluster, and
//! a constructed \c tobjrepo that represents this node's copy of the cluster
//! object repository.
//!
//! The constructor removes all the done files for this node. This removes
//! the done files for transactions that have been cancelled, but their
//! done file was not removed. The master sends a commit or a cancel to this
//! node, but the node stops before this is acted on. When quorum is regained,
//! each transaction gets validated and processed anew [INITREMOVEDONE].
//!
//! This is a superclass of clusterinfoObj. The constructor reads the cluster
//! node information from the repository, the configuration is saved in a
//! file. The constructor fetches it out, and uses it to construct the initial
//! configuration for clusterinfoObj's constructor [INITCONFIG].
//!
//! Any time clusterinfoObj reports that the cluster's master has changed,
//! create_master_controller() or create_slave_controller() gets invoked.
//! The subclass is responsible for instantiating the appropriate controller
//! object [CONTROLLER].
//!
//! After construction, initialize() must be invoked. This install an internal
//! callback for cluster updates, and
//! starts the first controller, likely via
//! create_master_controller() [INITIALCONTROLLER].
//!
//! The subclass must also inherit from \c x::stoppableObj. If the current
//! controller fails for some reason, the subclass's stop() method gets
//! invoked [ABORT].
//!
//! installQuorumNotification() installs a callback object that gets invoked
//! when the current repository controller announces that a quorum "might" be
//! present [QUORUMCB].
//!
//! Attempt to install a peer whose initial status claims that this node is
//! its master will be rejected [FALSEMASTER].
//!
//! \par Node ordering
//!
//! loadnodeorder() and savenodeorder() provide methods for arranging the
//! metadata of nodes in the cluster in a predetermine order, according to their
//! ordernum() [NODEORDER].
//!
//! \par Forced master election, and full quorum enforcement.
//!
//! master_handover_request() forces the requested node to become the cluster's
//! master. This method takes the name of the new master and a boolref.
//! master_handover_request() returns a mcguffin. When no references to the
//! mcguffin remain, the cluster should be in full quorum and boolref's status
//! indicates whether the forced election succeeded [MASTERHANDOVERREQUEST].
//!
//! full_quorum() returns a mcguffin that goes out of scope when the current
//! cluster is in full quorum, if it's not already in the full quorum
//! [FULLQUORUM].
//!
//! resign() forces a master election to make sure that this node is not
//! a master. resign() chooses a new master, and essentially uses
//! master_handover_request() to make the new node a master. If this node is
//! not a master, resign() essentially returns the result from full_quorum().
//! If this node is a master, the new master will be the previous
//! node in the cluster, before this node, that's eligible to become a master
//! [RESIGN].
//!
//! A failed boolref status will occur for one of the following reasons:
//!
//! - at the time one of these methods was invoked, the cluster is not
//! already in full quorum. The cluster must be in full quorum before an
//! election can be forced.
//!
//! - at the time an election was in progress, one or more nodes went down and
//! the cluster could not recover from the loss of the nodes in a manner
//! that maintained at least a majority quorum, all the time.
//!
//! - a failure reported by full_quorum() indicates that the cluster,
//! essentially, lost a majority quorum after full_quorum() returned, and
//! before the full quorum is now finally regained.

class repoclusterinfoObj : public clusterinfoObj {

	//! The logger object.
	LOG_CLASS_SCOPE;

	//! Open an object in the repository, directory

	static x::fdptr opendirect(const tobjrepo &repo,
				   const std::string &objname);

protected:
	//! The object repository
	tobjrepo repo;

public:
	//! Load cluster configuration from the object repository

	static STASHER_NAMESPACE::nodeinfomap
	loadclusterinfo(const tobjrepo &repo);


	//! Save cluster configuration from the object repository

	//! This is mostly used for debugging purposes.

	static void saveclusterinfo(const tobjrepo &repo,
				    const STASHER_NAMESPACE::nodeinfomap &infoArg);


	//! Constructor
	repoclusterinfoObj(//! Name of this node
			   const std::string &nodename,

			   //! Name of this cluster
			   const std::string &clustername,

			   //! Thread tracker
			   const x::ptr<STASHER_NAMESPACE::stoppableThreadTrackerObj>
			   &thread_trackerArg,

			   //! The repository object
			   const tobjrepo &repoArg);


	//! Destructor
	~repoclusterinfoObj() noexcept;

	//! Install an internal updater for cluster configuration
	void initialize();

	class configNotifierObj;

	friend class configNotifierObj;

	//! Return etc/maxobjects
	size_t getmaxobjects();

	//! Return etc/maxobjectsize
	size_t getmaxobjectsize();

private:
	//! Notifier for cluster updates, installed by initialize()
	x::ptr<configNotifierObj> updater;

	//! Implement cluster configuration update
	void update();

protected:
	//! Override installpeer_locked()

	//! A newly-connected peer should not claim that this node is its
	//! master.

	std::pair<bool, x::ptr<peerstatusObj> >
	installpeer_locked(const std::string &nodename,
			   const x::ptr<peerstatusObj> &node);


	//! Override peernewmaster()

	//! And here's a status update from an existing peer, which also
	//! gets forwarded to the current repository controller.

	virtual void peernewmaster(//! This is expected to be a
				   //! repopeerconnectionObj, really.
				   const x::ptr<peerstatusObj> &peerRef,

				   //! It's new master
				   const nodeclusterstatus &peerStatus);


private:

	//! Start the master (or maybe the slave), new cluster status given
	void startmaster(const newnodeclusterstatus &newStatus);


	class mcguffinDestroyCallbackObj;

	//! Thread-safe container for the current master or slave.
	typedef x::mpobj<x::ptr<repocontrollerbaseObj>> curmaster_t;

	//! Current master (or slave)
	curmaster_t curmaster;

public:

	//! Return the current controller

	x::ptr<repocontrollerbaseObj> getCurrentController();


private:
	//! A mcguffin that triggers invocation of this object's stop(),
	//! upon destruction.
	x::weakptr<x::ptr<x::obj> > mcguffin;

	//! Quorum callback list
	repoclusterquorum quorum_callback_list;


	//! Subclass must instantiate a new repository controller in master mode
	virtual repocontroller_start_info
	create_master_controller(//! Master's name, this node's name
				 const std::string &mastername,

				 //! Master's uuid
				 const x::uuid &masteruuid,

				 //! The repository
				 const tobjrepo &repo,

				 //! List of registered quorum callbacks
				 const repoclusterquorum &callback_listArg)
=0;

	//! Subclass must instantiate a new repository controller in slave mode
	virtual repocontroller_start_info
	create_slave_controller(//! Name of this node's object
				const std::string &mastername,

				//! The connection to the master peer

				//! Only weak references to this object can be retained
				const x::ptr<peerstatusObj> &peer,

				//! Master's uuid
				const x::uuid &masteruuid,

				//! This node's repository
				const tobjrepo &repo,

				//! List of registered quorum callbacks
				const repoclusterquorum &callback_listArg)
=0;

public:
	//! Install a callback for quorum notification.
	void installQuorumNotification(const x::ref<STASHER_NAMESPACE::
						    repoclusterquorumcallbackbaseObj> &notifier);

	//! Return current controller's quorum status (for debugging purposes only)
	STASHER_NAMESPACE::quorumstateref debug_inquorum();


	//! Transfer master status to another node

	//! If this node is a master, make another node a master, instead.
	//!
	//! \return A mcguffin. All references to the mcguffin are released when
	//! the transfer is complete, or if it fails for any reason.

	x::ref<x::obj>
	master_handover_request(//! Which node should now be a master
				const std::string &newmaster,

				//! Once the mcguffin goes out of scope, this
				//! indicates whether the transfer succeeded.
				const boolref &succeeded);

	//! If this node is a master, resign.

	//! If this node is a master, select another node in the cluster
	//! and invoke master_handover_request. If not, a mcguffin gets returned
	//! that will go out of scope when the cluster is in quorum (which may
	//! be immediately, if the cluster is already in quorum).
	//!
	//! succeeded will wind up getting set to false if there was a master
	//! handover from this node, but it failed, or if at the time
	//! resign() was invoked there was a master election in progress; in
	//! either case resign() should be invoked again (but onlyafter the
	//! returner mcguffin goes out of scope).

	x::ref<x::obj> resign(const boolref &succeeded);


	//! Wait until the cluster is in full quorum

	x::ref<x::obj> full_quorum(const boolref &succeeded);

private:
	//! Initiate a handover monitoring thread

	//! \internal starts an internal thread that waits until a mcguffin
	//! goes out of scope, and until the cluster is in quorum, then checks
	//! that the cluster's master is the expected cluster master.
	//!
	//! \return a mcguffin for the whole operation, which is really the
	//! internal thread object, which goes out of scope when the thread
	//! is finished. This is the mcguffin that gets returned by
	//! master_handover_request(), resign, and full_quorum().

	x::ref<x::obj> start_handover_thread(const std::string &newmaster,
					     const boolref &succeeded,
					     const x::ptr<x::obj> &mcguffin);

};
#endif

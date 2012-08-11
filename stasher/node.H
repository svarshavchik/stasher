/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef node_H
#define node_H

#include "tobjrepo.H"
#include "stoppablethreadtracker.H"
#include "clusterlistenerimpl.H"
#include "trandistributor.H"
#include "repoclusterinfoimpl.H"

#include <x/logger.H>
#include <x/threads/timer.H>

//! Top level node object

//! Create top-level objects that run the node.
//!
//! After constructing, invoke start() to start the threads.
//!
//! This is a one-shot deal. The object cannot be reused.

class node {

	//! Logger

	LOG_CLASS_SCOPE;

	class quorumcbObj;

	//! Mcguffin used for stopping the node

	class stop_cb;

	//! Certificate reload monitor object

	class reloadNotifyObj;
public:
	//! The repository object where cluster root certificates are distributed from
	static const char rootcerts[];

	//! The transactional repository manager
	tobjrepoptr repo;

	//! Stoppable thread tracker implementation
	STASHER_NAMESPACE::stoppableThreadTrackerImplptr trackerimpl;

	//! Thread starter and stopper

	STASHER_NAMESPACE::stoppableThreadTrackerptr tracker;

	//! Listener thread
	clusterlistenerimplptr listener;

	//! Distributor thread
	x::ptr<trandistributorObj> distributor;

	//! Cluster metadata object.
	repoclusterinfoimplptr repocluster;

	//! The timer wrapper kills the timer in the destructor

	class reconnecter : virtual public x::obj {

	public:
		//! The timer thread

		x::timer timer;

		//! Constructor
		reconnecter(const std::string &name)
			: timer(x::timer::create())
		{
			timer->setTimerName(name);
		}

		//! Destructor
		~reconnecter() noexcept
		{
			timer->cancel();
		}

		//! The internal job that periodically invokes pingallpeers() and connectpeers()

		class LIBCXX_HIDDEN job;
	};

	class certcheck_job;

	//! The reconnecter thread timer

	x::ptr<reconnecter> reconnecter_thread;

	//! Notifier object reloads certificates when they're updated
	x::ptr<reloadNotifyObj> certnotifier;

	//! Default constructor
	node();

	//! Constructor
	node(const std::string &dir);

	//! Explicit declaration due to non-visible reloadNotifyObj
	node(const node &);

	//! Explicit declaration due to non-visible reloadNotifyObj
	node &operator=(const node &);

	//! Destructor
	~node() noexcept;

	class distributorMonitorObj;

	//! Start all the threads
	void start(//! If set to true, do not start the reconnecter thread

		   //! This is used by regression tests
		   bool noreconnecter);

	//! Start the reconnecter thread

	//! This is used by some regression tests, after start(true).
	//!
	void start_reconnecter();

	//! Wait for the node to stop

	void wait();

	//! For regression tests - wait for a full quorum to be established or un-established

	void debugWaitFullQuorumStatus(bool flag);

	//! For regression tests - wait for a majority quorum to be established or un-established

	void debugWaitMajorityQuorumStatus(bool flag);

	//! For regression tests - wait for all connections to come up

	void debugWait4AllConnections();

	class wait4allconnections;
};

#endif

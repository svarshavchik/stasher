/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef repoclusterinfoimpl_H
#define repoclusterinfoimpl_H

#include "repoclusterinfoimplfwd.H"
#include "stoppablethreadtrackerfwd.H"
#include "repoclusterinfo.H"
#include "adminstopint.H"
#include <x/stoppable.H>
#include <x/ref.H>
#include <x/weakptr.H>
#include <x/logger.H>
#include <x/mpobj.H>

#include "stasher/namespace.H"

class trandistributorObj;

//! Default implementation of repoclusterinfoObj virtual methods.

//! This class provides the default implementations of repoclusterinfoObj's
//! virtual methods. Additionally, it multiply-inherits from x::stoppableObj.
//!
//! wait() waits for stop() to be invoked.
//!
//! An instantiated master or slave controller gets a stoppable interface,
//! that's tied into this x::stoppableObj instance. If the master or the
//! slave controller invokes the stoppable interface's stop(), this node's
//! stop() gets invoked. This is used to implement HALT. [HALTCLUSTERSTOP]

class repoclusterinfoimplObj : public repoclusterinfoObj,
			       public adminstopintObj,
			       public x::stoppableObj {

	//! A stoppable interface that's passed to the controllers.

	//! Stopping the node is done by calling my stop(). To prevent
	//! circular references, we need to use a weak pointer.

	class haltstopObj : virtual public x::stoppableObj {

		//! The repoclusterinfoimpl instance

		x::weakptr<x::stoppableptr> realstop;

	public:
		//! Constructor
		haltstopObj(const x::stoppableptr &me);

		//! Destructor
		~haltstopObj();

		//! Invoke stop() on repoclusterinfoimplObj

		void stop();
	};

	//! The logger object
	LOG_CLASS_SCOPE;

	//! The tracker for starting threads

	STASHER_NAMESPACE::stoppableThreadTracker tracker;

	//! The distributor object

	x::weakptr<x::ptr<trandistributorObj> > distributor;

	//! A container for the stop flag

	typedef x::mpcobj<bool> stopflag_t;

	//! stop() was invoked
	stopflag_t stopflag;

public:
	//! Constructor
	repoclusterinfoimplObj(//! Name of this node
			       const std::string &nodename,

			       //! Name of this cluster
			       const std::string &clustername,

			       //! The object repository
			       const tobjrepo &repoArg,

			       //! The distributor object
			       const x::ptr<trandistributorObj> &distributorArg,

			       //! Tracker, for starting threads
			       const STASHER_NAMESPACE::stoppableThreadTracker
			       &trackerArg);


	//! Destructor
	~repoclusterinfoimplObj();

private:

	//! Instantiate a master controller

	//! This default implementation instantiates a repocontrollermasterObj
	//! thread.

	repocontroller_start_info
	create_master_controller(const std::string &mastername,
				 const x::uuid &masteruuid,
				 const tobjrepo &repo,
				 const repoclusterquorum &callback_listArg)
		override;


	//! Instantiate a slave controller

	//! This default implementation instantiates a repocontrollerslaveObj
	//! thread.

	repocontroller_start_info
	create_slave_controller(const std::string &mastername,
				const x::ptr<peerstatusObj> &peer,
				const x::uuid &masteruuid,
				const tobjrepo &repo,
				const repoclusterquorum &callback_listArg)
		override;


public:
	//! Stop this instance.
	void stop();

	//! Implement admin stop interface

	void stop(const x::fd &socket);

	//! Get the file descriptor that sent the admin stop command

	x::fdptr getstopfd();

	//! Wait for this instance to stop.
	void wait();

private:
	//! Stashed file descriptor

	//! When admin thread wants to stop this node, its file descriptor
	//! gets stashed here, so it only goes out of scope and gets closed
	//! after all the threads stop.
	x::fdptr stop_fd;

};

#endif

/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "repoclusterinfoimpl.H"
#include "stoppablethreadtracker.H"
#include "repocontrollermaster.H"
#include "repocontrollerslave.H"
#include "peerstatus.H"
#include "trandistributor.H"

LOG_CLASS_INIT(repoclusterinfoimplObj);

repoclusterinfoimplObj::haltstopObj::haltstopObj(const x::stoppableptr &me)
	: realstop(me)
{
}

repoclusterinfoimplObj::haltstopObj::~haltstopObj() noexcept
{
}

void repoclusterinfoimplObj::haltstopObj::stop()
{
	x::stoppableptr ptr=realstop.getptr();

	if (!ptr.null())
	{
		ptr->stop();
	}
}

repoclusterinfoimplObj::repoclusterinfoimplObj(const std::string &nodename,
					       const std::string &clustername,
					       const tobjrepo &repoArg,
					       const x::ptr<trandistributorObj>
					       &distributorArg,
					       const STASHER_NAMESPACE::stoppableThreadTracker
					       &trackerArg)
	: repoclusterinfoObj(nodename, clustername,
			     trackerArg, repoArg),
	  tracker(trackerArg),
	  distributor(distributorArg),
	  stopflag(false)
{
}

repoclusterinfoimplObj::~repoclusterinfoimplObj() noexcept
{
}

repocontroller_start_info repoclusterinfoimplObj
::create_master_controller(const std::string &mastername,
			   const x::uuid &masteruuid,
			   const tobjrepo &repo,
			   const repoclusterquorum &callback_listArg)

{
	LOG_DEBUG("Creating master controller on "
		  << nodename << " for " << mastername);

	auto controller=repocontrollermaster::create
		(mastername, masteruuid, repo,
		 callback_listArg, distributor.getptr(), tracker,
		 x::ref<haltstopObj>::create(x::stoppableptr(this)));

	auto start_info=repocontroller_start_info::create(controller);

	controller->initialize(clusterinfo(this));
	return start_info;
}

repocontroller_start_info repoclusterinfoimplObj
::create_slave_controller(const std::string &mastername,
			  const x::ptr<peerstatusObj> &peer,
			  const x::uuid &masteruuid,
			  const tobjrepo &repo,
			  const repoclusterquorum &callback_listArg)

{
	LOG_DEBUG("Creating slave controller on "
		  << nodename << " for " << mastername);

	auto slave=x::ref<repocontrollerslaveObj>
		::create(mastername, peer,
			 masteruuid, repo,
			 callback_listArg,
			 distributor.getptr(),
			 tracker,
			 x::ref<haltstopObj>::create(x::stoppableptr(this)));

	return repocontroller_start_info::create(slave);
}

void repoclusterinfoimplObj::stop()
{
	stopflag_t::lock lock(stopflag);
	*lock=true;
	lock.notify_all();
}

void repoclusterinfoimplObj::wait()
{
	stopflag_t::lock lock(stopflag);

	while (!*lock)
		lock.wait();
}


void repoclusterinfoimplObj::stop(const x::fd &socket)
{
	{
		stopflag_t::lock lock(stopflag);

		stop_fd=socket;
	}
	stop();
}

x::fdptr repoclusterinfoimplObj::getstopfd()
{
	stopflag_t::lock lock(stopflag);

	return stop_fd;
}

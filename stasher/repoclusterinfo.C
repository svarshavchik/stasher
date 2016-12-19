/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "repoclusterinfo.H"
#include "repocontrollerbase.H"
#include "repopeerconnection.H"
#include "peerstatus.H"
#include "batonhandoverthread.H"
#include "boolref.H"
#include "newtran.H"
#include "trancommit.H"
#include "stasher/client.H"

#include <x/stoppable.H>
#include <x/serialize.H>
#include <x/deserialize.H>
#include <x/fditer.H>
#include <x/destroycallbackflag.H>

#include <algorithm>

LOG_CLASS_INIT(repoclusterinfoObj);

x::fdptr repoclusterinfoObj::opendirect(const tobjrepo &repo,
					const std::string &objname)
{
	std::set<std::string> objects, notfound;

	objrepoObj::values_t objectfiles;

	objects.insert(objname);
	repo->values(objects, true, objectfiles, notfound);

	objrepoObj::values_t::iterator iter(objectfiles.find(objname));

	if (iter == objectfiles.end())
		return x::fdptr();

	return iter->second.second;
}

STASHER_NAMESPACE::nodeinfomap
repoclusterinfoObj::loadclusterinfo(const tobjrepo &repo)
{
	STASHER_NAMESPACE::nodeinfomap m;

	try
	{
		x::fdptr obj=opendirect(repo, STASHER_NAMESPACE::client::base
					::clusterconfigobj);

		if (!obj.null())
		{
			x::fd::base::inputiter beg_iter(obj), end_iter;

			x::deserialize
				::iterator<x::fd::base::inputiter>(beg_iter,
								   end_iter)
				(m);
		}

	} catch (const x::exception &e)
	{
		LOG_FATAL(e);
		throw EXCEPTION("Cluster configuration load failure");
	}
	return m;
}

void repoclusterinfoObj::saveclusterinfo(const tobjrepo &repo,
					 const STASHER_NAMESPACE::nodeinfomap &infoArg)
{
	newtran tr(repo->newtransaction());

	x::fd fd(tr->newobj(STASHER_NAMESPACE::client::base::clusterconfigobj));

	{
		x::fd::base::outputiter fditer(fd);

		x::serialize
			::iterator<x::fd::base::outputiter> ser_iter(fditer);

		ser_iter(infoArg);

		fditer.flush();
	}
	fd->close();

	x::eventfd eventfd(x::eventfd::create());

	x::uuid uuid=tr->finalize();

	trancommit tran(repo->begin_commit(uuid, eventfd));

	while (!tran->ready())
		eventfd->event();

	tran->commit();
	repo->cancel(uuid);
}

repoclusterinfoObj::repoclusterinfoObj(const std::string &nodename,
				       const std::string &clustername,
				       const x::ptr<STASHER_NAMESPACE::stoppableThreadTrackerObj>
				       &thread_trackerArg,
				       const tobjrepo &repoArg)
	: clusterinfoObj(nodename, clustername,
			 thread_trackerArg,
			 loadclusterinfo(repoArg)),
	  repo(repoArg),
	  quorum_callback_list(repoclusterquorum::create())
{
	repo->cancel_done(nodename);
}

repoclusterinfoObj::~repoclusterinfoObj() noexcept
{
}

size_t repoclusterinfoObj::getmaxobjects()
{
	size_t cnt=10;

	x::fdptr obj=opendirect(repo, STASHER_NAMESPACE::client::base
				::maxobjects);

	if (!obj.null())
	{
		x::istream i(obj->getistream());

		(*i) >> cnt;
	}

	if (cnt < 5)
		cnt=5;

	return cnt;
}

size_t repoclusterinfoObj::getmaxobjectsize()
{
	size_t cnt=1024 * 1024 * 32;

	x::fdptr obj=opendirect(repo, STASHER_NAMESPACE::client::base
				::maxobjectsize);

	if (!obj.null())
	{
		x::istream i(obj->getistream());

		(*i) >> cnt;
	}

	if (cnt < 1024)
		cnt=1024;

	return cnt;
}

class repoclusterinfoObj::configNotifierObj : public objrepoObj::notifierObj {

	x::weakptr<repoclusterinfoptr> myrepo;

public:
	configNotifierObj( const repoclusterinfo &myrepoArg);

	~configNotifierObj() noexcept;

	void installed(const std::string &objname,
		       const x::ptr<x::obj> &lock)
;

	void removed(const std::string &objname,
		     const x::ptr<x::obj> &lock)
;
};

repoclusterinfoObj::configNotifierObj
::configNotifierObj( const repoclusterinfo &myrepoArg)
	: myrepo(myrepoArg)
{
}

repoclusterinfoObj::configNotifierObj::~configNotifierObj() noexcept
{
}

void repoclusterinfoObj::configNotifierObj::installed(const std::string
						      &objname,
						      const x::ptr<x::obj>
						      &lock)
{
	if (objname == STASHER_NAMESPACE::client::base::clusterconfigobj)
	{
		repoclusterinfoptr repo(myrepo.getptr());

		if (!repo.null())
			repo->update();
	}
}

void repoclusterinfoObj::configNotifierObj::removed(const std::string &objname,
						    const x::ptr<x::obj> &lock)
{
	installed(objname, lock);
}

void repoclusterinfoObj::initialize()
{
	update();

	status current_status(clusterinfo(this));

	updater=x::ptr<configNotifierObj>::create(repoclusterinfo(this));

	repo->installNotifier(updater);

	startmaster(*current_status);
}

void repoclusterinfoObj::update()
{
	try {
		clusterinfoObj::update(loadclusterinfo(repo));
	} catch (const x::exception &e)
	{
		LOG_ERROR(STASHER_NAMESPACE::client::base::clusterconfigobj
			  << ": " << e);
	}
}

std::pair<bool, peerstatus>
repoclusterinfoObj::installpeer_locked(const std::string &name,
				       const peerstatus &node)
{
	LOG_INFO(nodename << ": connection from " << name << ", status: "
		 << ({
				 std::ostringstream o;

				 peerstatusObj::status(node)->toString(o);

				 o.str(); }));

	if (peerstatusObj::status(node)->master == nodename)
	{
		LOG_FATAL("Newly connected node " << nodename
			  << " claims that I, "
			  << getthisnodename()
			  << ", is already its master");
		return std::make_pair(false, peerstatus());
	}
	return clusterinfoObj::installpeer_locked(name, node);
}

void repoclusterinfoObj::peernewmaster(const x::ptr<peerstatusObj> &peerRef,
				       const nodeclusterstatus &peerStatus)
{
	{
		std::lock_guard<std::mutex> lock(objmutex);

		curmaster->peernewmaster(peerRef, peerStatus);
	}

	clusterinfoObj::peernewmaster(peerRef, peerStatus);
}

void repoclusterinfoObj
::installQuorumNotification(const x::ref<STASHER_NAMESPACE::
					 repoclusterquorumcallbackbaseObj>
			    &notifier)
{
	quorum_callback_list->install(notifier);
}

STASHER_NAMESPACE::quorumstateref
repoclusterinfoObj::debug_inquorum()
{
	STASHER_NAMESPACE::quorumstateref status=
		STASHER_NAMESPACE::quorumstateref::create();

	bool flag;

	do
	{
		x::ptr<x::obj> mcguffin(x::ptr<x::obj>::create());

		boolref processed(boolref::create());

		{
			std::lock_guard<std::mutex> lock(objmutex);

			if (curmaster.null())
				return status;

			curmaster->get_quorum(status, processed, mcguffin);
		}

		x::destroyCallbackFlag cb(x::destroyCallbackFlag::create());

		mcguffin->ondestroy([cb]{cb->destroyed();});
		mcguffin=x::ptr<x::obj>();

		cb->wait();
		flag=processed->flag;

	} while (!flag);

	return status;
}

class repoclusterinfoObj::mcguffinDestroyCallbackObj : virtual public x::obj {

public:
	x::weakptr< x::stoppableptr > terminator;
	bool stopping;

	mcguffinDestroyCallbackObj() noexcept;
	~mcguffinDestroyCallbackObj() noexcept;

	void destroyed();
};

repoclusterinfoObj::mcguffinDestroyCallbackObj::mcguffinDestroyCallbackObj()
	noexcept
{
}

repoclusterinfoObj::mcguffinDestroyCallbackObj::~mcguffinDestroyCallbackObj()
	noexcept
{
}

void repoclusterinfoObj::mcguffinDestroyCallbackObj::destroyed()
{
	x::stoppableptr stopthis(terminator.getptr());

	if (!stopthis.null())
		stopthis->stop();
}

void repoclusterinfoObj::startmaster(const newnodeclusterstatus &newStatus)
{
	repocontrollerbase controller=
		newStatus.status.master == nodename ?
		create_master_controller(newStatus.status.master,
					 newStatus.status.uuid, repo,
					 quorum_callback_list) :
		create_slave_controller(newStatus.status.master,
					newStatus.masterpeer,
					newStatus.status.uuid, repo,
					quorum_callback_list);

	LOG_DEBUG("Created controller on " << nodename
		  << ": " << &*x::ptr<x::obj>(controller));


	{
		std::lock_guard<std::mutex> lock(objmutex);

		if (!curmaster.null())
		{
			LOG_DEBUG("Replaced controller on " << nodename
				  << ": " << &*x::ptr<x::obj>(curmaster));

			curmaster->handoff(controller);
			curmaster=controller;
			return;
		}
	}

	// First one

	x::ptr<mcguffinDestroyCallbackObj>  mcguffin_destroy_callback=
		x::ptr<mcguffinDestroyCallbackObj>::create();

	mcguffin_destroy_callback->terminator=
		x::stoppable(x::ref<repoclusterinfoObj>(this));

	x::ptr<x::obj> mcguffin(x::ptr<x::obj>::create());

	mcguffin->ondestroy([mcguffin_destroy_callback]
			    {
				    mcguffin_destroy_callback->destroyed();
			    });

	this->mcguffin=mcguffin;

	this->curmaster=controller;

	controller->start_controller(mcguffin);
}

x::ptr<repocontrollerbaseObj> repoclusterinfoObj::getCurrentController()
{
	std::lock_guard<std::mutex> lock(objmutex);

	return curmaster;
}

x::ref<x::obj> repoclusterinfoObj::
master_handover_request(const std::string &newmaster,
			const boolref &succeeded)
{
	return start_handover_thread(newmaster,
				     succeeded,
				     getCurrentController()->
				     handoff_request(newmaster));
}

x::ref<x::obj> repoclusterinfoObj::
start_handover_thread(const std::string &newmaster,
		      const boolref &succeeded,
		      const x::ptr<x::obj> &mcguffin)
{
	x::ref<batonhandoverthreadObj>
		thr(x::ref<batonhandoverthreadObj>::create());

	thread_tracker->start_thread(thr,
				     newmaster, succeeded,
				     x::weakptr<x::ptr<x::obj> > (mcguffin),
				     quorum_callback_list,
				     clusterinfo(this));

	return thr;
}

x::ref<x::obj> repoclusterinfoObj::resign(const boolref &succeeded)
{
	STASHER_NAMESPACE::nodeinfomap clusterinfo;

	get(clusterinfo);

	std::vector<STASHER_NAMESPACE::nodeinfomap::iterator > nodeorder;

	STASHER_NAMESPACE::nodeinfo::loadnodeorder(clusterinfo, nodeorder);

	// Iterate over nodes in their cluster order, set next_master to the
	// last node just before this node that does not have nomaster() set.
	// If this node is the first node with nomaster() set, set next_master
	// to the last node in the cluster that does not have nomaster() set.

	STASHER_NAMESPACE::nodeinfomap::iterator next_master=clusterinfo.end();

	for (std::vector<STASHER_NAMESPACE::nodeinfomap::iterator>::iterator
		     b=nodeorder.begin(),
		     e=nodeorder.end(); b != e; ++b)
	{
		if ( (*b)->first == nodename &&
		     next_master != clusterinfo.end())
			break;

		if (!(*b)->second.nomaster())
			next_master=*b;
	}

	std::string newmaster;
	x::ptr<x::obj> mcguffin;

	x::ptr<repocontrollerbaseObj> controller=getCurrentController();

	if (dynamic_cast<repocontrollermasterObj *>(&*controller) == NULL)
	{
		// If this node is not a master, set up a dummy mcguffin that
		// will, essentially, result in the handover thread simply
		// waiting until the current cluster is in quorum.
		mcguffin=x::ptr<x::obj>::create();
		newmaster=controller->mastername;
	}
	else
	{
		newmaster=next_master->first;
		mcguffin=controller->handoff_request(newmaster);

	}

	return start_handover_thread(newmaster, succeeded, mcguffin);
}

x::ref<x::obj> repoclusterinfoObj::full_quorum(const boolref &succeeded)

{
	return start_handover_thread(getCurrentController()->mastername,
				     succeeded, x::ptr<x::obj>::create());
}

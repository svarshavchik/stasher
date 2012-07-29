/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "clusterinfo.H"
#include "clusterstatusnotifier.H"
#include "clusternotifier.H"
#include "peerstatus.H"
#include "stoppablethreadtracker.H"
#include "repopeerconnection.H"
#include "baton.H"
#include <x/logger.H>
#include <x/destroycallbackobj.H>

class clusterinfoObj::cluster_notification {

public:

	inline static void event(const clusternotifier &notifier,
				 const clusterinfoObj::cluster_t &arg)

	{
		notifier->clusterupdated(arg);
	}

};

class clusterinfoObj::node_initial_notification {

public:

	inline static void event(const clusterstatusnotifier &notifier,
				 const nodeclusterstatus &arg)

	{
		notifier->initialstatus(arg);
	}
};

class clusterinfoObj::node_update_notification {

public:

	inline static void event(const clusterstatusnotifier &notifier,
				 const nodeclusterstatus &arg)

	{
		notifier->statusupdated(arg);
	}
};


clusterinfoObj::newnodeclusterstatus
::newnodeclusterstatus(const nodeclusterstatus &statusArg)
 : status(statusArg)
{
}

clusterinfoObj::newnodeclusterstatus
::~newnodeclusterstatus() noexcept
{
}

clusterinfoObj::clusterinfoObj(const std::string &nodenameArg,
			       const std::string &clusternameArg,
			       const STASHER_NAMESPACE::stoppableThreadTracker
			       &thread_trackerArg,
			       const STASHER_NAMESPACE::nodeinfomap
			       &clusterinfo)

	: nodename(nodenameArg), clustername(clusternameArg),
	  thread_tracker(thread_trackerArg),
	  connection_attempts(connection_attempts_t::create())
{
	do_update(clusterinfo, false);

	nodeclusterstatus s=
		calculate_status_locked(vipcluster_t::readlock(cluster))
		.status;

	*thisnodestatus_t::writelock(thisnodestatus)=s;
}

clusterinfoObj::~clusterinfoObj() noexcept
{
}

void clusterinfoObj::get(STASHER_NAMESPACE::nodeinfomap &clusterinfo)

{
	vipcluster_t::readlock rlock(cluster);

	clusterinfo.clear();
	clusterinfo.insert(rlock->begin(), rlock->end());
	clusterinfo.insert(std::make_pair(nodename, thisnode));
}

void clusterinfoObj::update(const STASHER_NAMESPACE::nodeinfomap
			    &newclusterinfo)

{
	do_update(newclusterinfo, true);
}

void clusterinfoObj::do_update(const STASHER_NAMESPACE::nodeinfomap
			       &newclusterinfo, bool do_recalculate)

{
	bool recalculate_needed=false;

	thisnodestatus_t::updatelock clock(thisnodestatus);

	{
		vipcluster_t::updatelock lock(cluster);

		do_update_locked(newclusterinfo, do_recalculate,
				 recalculate_needed);

		if (recalculate_needed && do_recalculate)
			lock.notify(*vipcluster_t::readlock(cluster));
	}

	if (recalculate_needed && do_recalculate)
		recalculate_locked(clock);
}

void clusterinfoObj::do_update_locked(const STASHER_NAMESPACE::nodeinfomap
				      &newclusterinfo,
				      bool do_recalculate,
				      bool &recalculate_needed)

{
	vipcluster_t::writelock peerlock(cluster);

	for (STASHER_NAMESPACE::nodeinfomap::const_iterator
		     b(newclusterinfo.begin()),
		     e(newclusterinfo.end()); b != e; ++b)
	{
		if (b->first == nodename)
		{
			// Always set this when we're called from constructor

			if (thisnode != b->second || !do_recalculate)
			{
				thisnode=b->second;
				recalculate_needed=true;
			}
			continue;
		}

		cluster_t::iterator cur_status=peerlock->find(b->first);

		if (cur_status == peerlock->end())
		{
			peerlock->insert(std::make_pair(b->first,
							nodeinfoconn
							(b->second)));
			recalculate_needed=true;
		}
		else
		{
			STASHER_NAMESPACE::nodeinfo &info=cur_status->second;

			if (info != b->second)
			{
				info=b->second;
				recalculate_needed=true;
			}
		}
	}

	for (cluster_t::iterator b(peerlock->begin()), e(peerlock->end()), p;
	     (p=b) != e; )
	{
		++b;

		if (newclusterinfo.find(p->first) == newclusterinfo.end())
		{
			peerstatus conn(p->second.connection.getptr());

			if (!conn.null())
				x::stoppable(conn)->stop();

			peerlock->erase(p);
			recalculate_needed=true;
		}
	}
}

LOG_FUNC_SCOPE_DECL(clusterinfoObj::installpeer, installpeerLog);

std::pair<bool, peerstatus>
clusterinfoObj::installpeer(const std::string &nodename,
			    const peerstatus &node)

{
	std::pair<bool, peerstatus> retval;

	installnotifyclusterstatus(node);

	{
		vipcluster_t::updatelock peerlock(cluster);

		retval=installpeer_locked(nodename, node);

		if (!retval.first)
			return retval;

		peerlock.notify(*vipcluster_t::readlock(cluster));
	}


	recalculate();
	return retval;
}

class clusterinfoObj::destroycb : public x::destroyCallbackObj {

	std::string name;

public:
	x::weakptr<clusterinfoptr> cluster;

	class updateThread;

	destroycb(const clusterinfo &clusterArg,
		  const std::string &nameArg) noexcept;
	~destroycb() noexcept;

	void destroyed() noexcept;
};

std::pair<bool, peerstatus>
clusterinfoObj::installpeer_locked(const std::string &nodename,
				   const peerstatus &node)

{
	LOG_FUNC_SCOPE(installpeerLog);

	vipcluster_t::writelock writelock(cluster);

	cluster_t::iterator p(writelock->find(nodename));

	if (p == writelock->end())
	{
		LOG_ERROR(nodename << ": no such peer");
		return std::make_pair(false, peerstatus());
	}

	peerstatus prevpeer(p->second.connection.getptr());

	if (!prevpeer.null())
	{
		LOG_WARNING(this->nodename << ": connection with "
			    << nodename << " already exists");
		return std::make_pair(false, prevpeer);
	}

	auto cb=x::ref<destroycb>::create(clusterinfo(this),
					  "Connection with " + nodename);

	node->addOnDestroy(cb);

	p->second.connection=node;

	LOG_INFO(this->nodename << ": connection with "
		 << nodename << " registered");
	return std::make_pair(true, peerstatus());
}

LOG_FUNC_SCOPE_DECL(clusterinfoObj::destroycb, destroycbLog);


class clusterinfoObj::destroycb::updateThread : public x::stoppableObj {

public:
	x::weakptr<clusterinfoptr> cluster;

	updateThread(const clusterinfo &clusterArg)
		: cluster(clusterArg)
	{
	}

	~updateThread() noexcept {}

	void stop() {}

	void run()
	{
		LOG_FUNC_SCOPE(destroycbLog);

		clusterinfoptr ptr(cluster.getptr());

		if (ptr.null())
			return;

		try {
			{
				// Do not combine these two, under penalty
				// of flogging, and being sent back to
				// "implementation defined evaluation order"
				// school.

				vipcluster_t::updatelock upd_lock(ptr->cluster);

				upd_lock.notify(*vipcluster_t
						::readlock(ptr->cluster));
			}

			ptr->recalculate();
		} catch (const x::exception &e)
		{
			LOG_FATAL(e);
			LOG_TRACE(e->backtrace);
		}
	}
};

clusterinfoObj::destroycb::destroycb(const clusterinfo &clusterArg,
				     const std::string &nameArg) noexcept
	: name(clusterArg->getthisnodename() + ": " + nameArg),
	  cluster(clusterArg)
{
}

clusterinfoObj::destroycb::~destroycb() noexcept
{
}

void clusterinfoObj::destroycb::destroyed() noexcept
{
	LOG_FUNC_SCOPE(destroycbLog);

	clusterinfoptr ptr(cluster.getptr());

	if (ptr.null())
		return;

	LOG_DEBUG(name + ": recalculating");

	ptr->thread_tracker->start(x::ref<updateThread>::create(ptr));
}

LOG_FUNC_SCOPE_DECL(clusterinfoObj::connectpeers, connectpeersLog);

void clusterinfoObj::connectpeers(const connectpeers_callback &callback)

{
	LOG_FUNC_SCOPE(connectpeersLog);

	vipcluster_t::readlock lock(cluster);

	LOG_DEBUG(nodename << ": making connection attempts to unconnected peers");

	for (cluster_t::const_iterator b(lock->begin()), e(lock->end());
	     b != e; ++b)
	{
		if (!b->second.connection.getptr().null())
			continue;

		if (connection_attempts->count(b->first) > 0)
		{
			LOG_TRACE(nodename << ": " << b->first << ": connection in progress");
			continue;
		}

		LOG_TRACE(nodename << ": " << b->first << ": connecting");
		connection_attempts->insert(std::make_pair(b->first,
							   callback(b->first))
					    );
	}
}

void clusterinfoObj::installnotifycluster(const clusternotifier &notifier)

{
	// Do not combine these two, under penalty
	// of flogging, and being sent back to
	// "implementation defined evaluation order"
	// school.

	vipcluster_t::handlerlock handler_lock(cluster);

	handler_lock.install(notifier, *vipcluster_t::readlock(cluster));
}

void clusterinfoObj::installnotifyclusterstatus(const clusterstatusnotifier
						&notifier)

{
	// Do not combine these two, under penalty
	// of flogging, and being sent back to
	// "implementation defined evaluation order"
	// school.

	thisnodestatus_t::handlerlock handler_lock(thisnodestatus);

	handler_lock.install(notifier,
			     *thisnodestatus_t::readlock(thisnodestatus));
}

void clusterinfoObj::peerstatusupdated(const x::ptr<peerstatusObj> &peerRef,
				       const nodeclusterstatus &peerStatus)

{
	recalculate();
}

void clusterinfoObj::peernewmaster(const x::ptr<peerstatusObj> &peerRef,
				   const nodeclusterstatus &peerStatus)

{
}

void clusterinfoObj::clearbaton(const baton &batonArg)

{
	thisnodestatus_t::updatelock lock(thisnodestatus);

	batonArg->cluster_cleared_flag=true;

	batonptr batonref=batonp.getptr();

	if (batonref.null())
		return;

	if (!batonref->cluster_cleared_flag)
		return;

	batonp=batonptr();
	recalculate_locked(lock);
}

LOG_FUNC_SCOPE_DECL(clusterinfoObj::installbaton, installbatonLog);

bool clusterinfoObj::installbaton(const baton &batonArg)

{
	LOG_FUNC_SCOPE(installbatonLog);

	LOG_TRACE(getthisnodename() << ": Acquiring lock");

	thisnodestatus_t::updatelock lock(thisnodestatus);

	LOG_TRACE(getthisnodename() << ": Lock acquired");

	std::string current_master=status(this)->master;

	LOG_TRACE(getthisnodename() << ": Current master is " << current_master);

	if (current_master != batonArg->resigningnode)
	{
		LOG_WARNING(getthisnodename() << ": Rejecting baton from "
			    << batonArg->resigningnode
			    << " because this node's current master is "
			    << current_master);
		return false;
	}

	// Note -- can't just clear temp_baton here.
	// Old master relies on temp_baton destructor to send out recall
	// messages, which we don't want to happen when the replacement baton
	// gets installed :-(.
	{
		batonptr cur_baton=batonp.getptr();

		if (!cur_baton.null() && !batonArg->temp_baton.null())
		{
			batonArg->temp_baton->is_replacement_baton=true;

			if (cur_baton->is_replacement_baton)
				cur_baton=batonptr();

			batonArg->temp_baton->is_replacement_baton=false;
		}

		if (!cur_baton.null())
		{
			LOG_ERROR(getthisnodename()
				  << ": Rejecting baton from "
				  << batonArg->resigningnode
				  << " because there's already a baton from "
				  << cur_baton->resigningnode);
			return false;
		}
	}

	auto destructor=x::ref<destroycb>::create(clusterinfo(this),
						  "Baton transfer: "
						  + (std::string)*batonArg);

	batonArg->addOnDestroy(destructor);

	batonp=batonArg;

	LOG_DEBUG(getthisnodename() << ": installed baton: "
		  << (std::string)*batonArg);
	recalculate_locked(lock);
	LOG_DEBUG(getthisnodename() << ": new status: "
		  << status(this)->master);
	return true;
}

batonptr clusterinfoObj::getbaton()
{
	thisnodestatus_t::readlock lock(thisnodestatus);

	return batonp.getptr();
}

void clusterinfoObj::recalculate()
{
	thisnodestatus_t::updatelock lock(thisnodestatus);

	recalculate_locked(lock);
}

LOG_FUNC_SCOPE_DECL(clusterinfoObj::recalculate_locked, recalculateLog);

void clusterinfoObj::recalculate_locked(thisnodestatus_t::updatelock
					&updatelock)

{
	LOG_FUNC_SCOPE(recalculateLog);

#ifdef DEBUG_RECALCULATE_HOOK
	DEBUG_RECALCULATE_HOOK();
#endif

	newnodeclusterstatus s=
		calculate_status_locked(vipcluster_t::readlock(cluster));

	bool new_master;

	{
		thisnodestatus_t::writelock statuslock(thisnodestatus);

		if (*statuslock == s.status)
		{
			LOG_DEBUG(nodename << ": status unchanged");
			return;
		}

		LOG_DEBUG(nodename << ": changed status to "
			  << ({ std::ostringstream o;

					  s.status.toString(o);
					  o.str(); }));

		new_master=statuslock->master != s.status.master;

		*statuslock=s.status;
	}

	updatelock.notify(s.status);

	if (new_master)
		startmaster(s);
}

void clusterinfoObj::startmaster(const newnodeclusterstatus &newStatus)

{
}

LOG_FUNC_SCOPE_DECL(clusterinfoObj::calculate_status_locked, calculateLog);

clusterinfoObj::newnodeclusterstatus clusterinfoObj
::calculate_status_locked(const vipcluster_t::readlock &clusterlock)


{
	LOG_FUNC_SCOPE(calculateLog);

	LOG_DEBUG(nodename << ": calculating node status");

	// Adjust current status if a baton is present

	nodeclusterstatus initialstatus=
		calculate_status_locked_with_baton(clusterlock);

	LOG_DEBUG(nodename << ": current status is "
		  << ({ std::ostringstream o;

				  initialstatus.toString(o);
				  o.str(); }));

	// Proposed status is the same, initially.
	newnodeclusterstatus status=initialstatus;

	// I'll assume I'm the master, unless told otherwise
	bool iammaster=true;
	cluster_t::const_iterator b, e;

	batonptr batonref=batonp.getptr();

	// Locate my current master (if I am a slave)
	if (initialstatus.master != nodename)
	{
		// If we claim that someone else is the master, this node
		// must actually exist.

		peerstatus n(getnodepeer_locked(clusterlock,
						initialstatus.master));

		// And it must be connected
		if (!n.null())
		{
			peerstatusObj::status cur_status(n);

			// Make sure that this node claims masterhood,
			// too.

			if (cur_status->master == initialstatus.master
			    && cur_status->ismasterof(initialstatus))
			{
				status.status= *cur_status;
				status.masterpeer=n;
				iammaster=false;
			}
		}

		// Otherwise, if the above tests fail, annoint myself a master.

		if (iammaster)
		{
			LOG_TRACE(nodename << ": no connection to master");

			if (!batonref.null())
			{
				LOG_INFO("Cancelling baton transfer to "
					 << batonref->replacementnode
					 << " because it doesn't think it's"
					 " a master");
				batonp=batonptr();
			}

			nodeclusterstatus new_status(nodename, x::uuid(),
						     0, false);

			new_status.timestamp=time(NULL);

			// I am the master, and set a new uuid.

			status.status=new_status;
		}
	}
	// else I'm really the master, so I assumed that iammaster=true
	// correctly.

	if (iammaster)
	{
		LOG_TRACE(nodename << ": counting slaves");

		// Ok, I'm the master. Count my slaves again.
		status.status.slavecount=0;

		// Update my reluctance flag.
		status.status.reluctant=thisnode.nomaster();

		for (b=clusterlock->begin(), e=clusterlock->end(); b != e; ++b)
		{
			peerstatus n(b->second.connection.getptr());

			if (n.null())
			{
				LOG_TRACE(nodename << ": not connected with "
					  << b->first);
				continue;
			}

			peerstatusObj::status cur_status(n);

			LOG_TRACE(nodename << ": peer status of "
				  << b->first << " is "
				  << ({ std::ostringstream o;

						  cur_status->toString(o);
						  o.str(); }));

			if (status.status.ismasterof(*cur_status))
				++status.status.slavecount;
		}
		LOG_TRACE(nodename << ": I have " << status.status.slavecount
			  << " slaves");
	}

	// The last step is overriden by presence of a baton.

	if (!batonref.null())
		return status;

	// Search for a better master than the current one, which may even be
	// me.

	for (b=clusterlock->begin(), e=clusterlock->end(); b != e; ++b)
	{
		peerstatus n(b->second.connection.getptr());

		if (n.null())
			continue;

		peerstatusObj::status cur_status(n);

		if (cur_status->master == nodename)
			continue; // It thinks I'm the master, so ignore.

		// This node is its own master, and it's status is better than
		// my current status

		if (cur_status->master == b->first
		    && *cur_status > status.status)
		{
			status.status= *cur_status;
			status.masterpeer=n;

			LOG_TRACE(nodename << ": found better master: "
				  << ({ std::ostringstream o;

						  cur_status->toString(o);
						  o.str(); }));

		}
	}
	return status;
}

LOG_FUNC_SCOPE_DECL(clusterinfoObj::calculate_status_locked_with_baton,
		    batonLog);

nodeclusterstatus clusterinfoObj
::calculate_status_locked_with_baton(const vipcluster_t::readlock &clusterlock)

{
	LOG_FUNC_SCOPE(batonLog);

	cluster_t::const_iterator b, e;

	nodeclusterstatus current_status(*status(this));

	batonptr batonref=batonp.getptr();

	if (batonref.null())
		return current_status;

	// If there's an installed baton, if the name of the baton's resigning
	// master is not this node or some other node in the cluster,
	// the baton gets removed.

	if (batonref->resigningnode != nodename &&
	    clusterlock->find(batonref->resigningnode) == clusterlock->end())
	{
		LOG_INFO(nodename << ": cancelling baton transfer from "
			 << batonref->resigningnode
			 << " because there's no such node in the cluster");
		batonp=batonptr();
		return current_status;
	}


	bool found_replacement_node=false;

	// If there's an installed baton, and the replacement node is this
	// node's name, and this node's status is not currently a master,
	// change this node's status to indicate that it's a master.

	nodeclusterstatus baton_status=current_status;

	if (batonref->replacementnode == nodename &&
	    baton_status.master != nodename)
	{
		baton_status=nodeclusterstatus(nodename, x::uuid(), 0, false);
		baton_status.timestamp=time(NULL);

		LOG_INFO(nodename
			 << ": Becoming a master due to a baton transfer from "
			 << batonref->resigningnode
			 << " to "
			 << batonref->replacementnode);
	}

	for (b=clusterlock->begin(), e=clusterlock->end(); b != e; ++b)
	{
		peerstatus n(b->second.connection.getptr());

		if (n.null())
		{
			// If there's an installed baton indicating that new
			// master is another peer in the cluster, and there's
			// no registered connection to the peer, the baton gets
			// removed.

			if (b->first == batonref->replacementnode)
			{
				LOG_INFO(nodename
					 << ": cancelling baton transfer to "
					 << batonref->replacementnode
					 << " because connection to it"
					 " has been lost");
				batonp=batonptr();
				return current_status;
			}

			continue;
		}

		peerstatusObj::status cur_status(n);

		if (b->first == batonref->replacementnode &&
		    cur_status->master == b->first)
		{
			found_replacement_node=true;
			baton_status= *cur_status;
		}

#if 0
		// If there's an installed baton, and any peer in the cluster
		// has a status that currently indicates that its master is not
		// the baton's resigning master or the new master, the baton
		// gets removed.

		if (cur_status->master != batonref->resigningnode &&
		    cur_status->master != batonref->replacementnode)
		{
			LOG_INFO(nodename << ": cancelling baton transfer from "
				 << batonref->resigningnode
				 << " to "
				 << batonref->replacementnode
				 << " because "
				 << b->first
				 << "'s master is "
				 << cur_status->master);
			batonp=batonptr();
			return current_status;
		}
#endif
	}

	// If the name of the baton's new master is not this node or some
	// other node in the cluster, the baton gets removed.

	if (batonref->replacementnode != nodename && !found_replacement_node)
	{
		LOG_INFO(nodename << ": cancelling baton transfer to "
			 << batonref->replacementnode
			 << " because there's no such node in the cluster");
		batonp=batonptr();
		return current_status;
	}

	if (baton_status.master != current_status.master)
	{
		LOG_INFO(nodename << ": accepting baton transfer from "
			 << batonref->resigningnode
			 << " to "
			 << batonref->replacementnode);
	}
	return baton_status;
}

void clusterinfoObj::getnodeinfo(const std::string &nodename,
				 STASHER_NAMESPACE::nodeinfo &infoRet)
{
	vipcluster_t::readlock clusterlock(cluster);

	cluster_t::const_iterator p(clusterlock->find(nodename));

	if (p == clusterlock->end())
		throw EXCEPTION(nodename + " does not exist");

	infoRet=p->second;
}

x::ptr<peerstatusObj> clusterinfoObj::getnodepeer(const std::string &nodename)

{
	return getnodepeer_locked(vipcluster_t::readlock(cluster), nodename);
}

x::ptr<peerstatusObj> clusterinfoObj
::getnodepeer_locked(const vipcluster_t::readlock &clusterlock,
		     const std::string &nodename)
{
	cluster_t::const_iterator p(clusterlock->find(nodename));

	if (p != clusterlock->end())
		return p->second.connection.getptr();

	return x::ptr<peerstatusObj>();
}

void clusterinfoObj::getthisnodeinfo(STASHER_NAMESPACE::nodeinfo &infoRet)
{
	vipcluster_t::readlock clusterlock(cluster);

	infoRet=thisnode;
}

void clusterinfoObj::getallpeers(std::list<peerstatus> &allpeerList)

{
	vipcluster_t::readlock clusterlock(cluster);

	for (cluster_t::const_iterator
		     b=clusterlock->begin(), e=clusterlock->end(); b != e; ++b)
	{
		peerstatus n(b->second.connection.getptr());

		if (!n.null())
			allpeerList.push_back(n);
	}
}

void clusterinfoObj::pingallpeers(const x::ptr<x::obj> &mcguffin)

{
	vipcluster_t::readlock clusterlock(cluster);

	for (cluster_t::const_iterator
		     b=clusterlock->begin(), e=clusterlock->end(); b != e; ++b)
	{
		peerstatus n(b->second.connection.getptr());

		if (!n.null())
		{
			repopeerconnectionObj *p=
				dynamic_cast<repopeerconnectionObj *>(&*n);

			if (p)
				p->ping(mcguffin);
		}
	}
}

void clusterinfoObj::installformermasterbaton(const baton &batonp)

{
	vipcluster_t::readlock clusterlock(cluster);

	for (cluster_t::const_iterator
		     b=clusterlock->begin(), e=clusterlock->end(); b != e; ++b)
	{
		peerstatus n(b->second.connection.getptr());

		if (!n.null())
		{
			repopeerconnectionObj *p=
				dynamic_cast<repopeerconnectionObj *>(&*n);

			if (p)
				p->installformermasterbaton(batonp);
		}
	}
}

void clusterinfoObj::report(std::ostream &o)
{
	bool locked;

	locked=false;

	try {
		thisnodestatus_t::readlock lock(thisnodestatus,
						std::try_to_lock_t());

		locked=true;

		if (lock.locked())
		{
			o << "  Status: ";

			lock->toString(o);
			o << std::endl;
		}
	} catch (const x::exception &e)
	{
		if (locked)
			throw;

		o << "  Unable to acquire status lock" << std::endl;
	}

	locked=false;

	// Strong references to connection objects should not go out of scope
	// while a vipcluster_t lock is being held.

	std::list<x::ptr<peerstatusObj> > temp_conn_list;

	try {
		vipcluster_t::writelock peerlock(cluster, std::try_to_lock_t());

		locked=true;

		if (peerlock.locked())
		{
			o << "  This node:" << std::endl;
			thisnode.toString(o);

			for (cluster_t::iterator b=peerlock->begin(),
				     e=peerlock->end(); b != e; ++b)
			{
				x::ptr<peerstatusObj>
					conn=b->second.connection.getptr();

				temp_conn_list.push_back(conn);

				o << "  Node: " << b->first
				  << ": " << (conn.null() ? "not connected":"connected")
				  << std::endl;
				b->second.toString(o);
			}
		}
	} catch (const x::exception &e)
	{
		if (locked)
			throw;

		o << "  Unable to acquire cluster lock" << std::endl;
	}
	temp_conn_list.clear();

	batonptr baton_ptr=batonp.getptr();

	if (!baton_ptr.null())
		o << "  Baton installed: " << (std::string)*baton_ptr
		  << std::endl;

	o << "  Connection attempts in progress:" << std::endl;

	for (auto conn: *connection_attempts)
		o << "    " << conn.first << std::endl;

}

void clusterinfoObj::debugUpdateTimestamp()
{
	thisnodestatus_t::writelock(thisnodestatus)->timestamp=time(NULL);
}

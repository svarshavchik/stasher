/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "clustertlsconnectshutdown.H"
#include "node.H"
#include "peerstatus.H"
#include "repomg.H"
#include "trancommit.H"
#include <x/destroy_callback.H>
#include <x/threads/timertask.H>
#include <x/mpobj.H>

LOG_CLASS_INIT(node);

const char node::rootcerts[]="etc/rootcerts";

class node::reloadNotifyObj : public objrepoObj::notifierObj {

public:
	std::string dir;

	tobjrepo repo;

	clusterlistenerimpl listener;

	reloadNotifyObj(const std::string &dirArg,
			const tobjrepo &repoArg,
			const clusterlistenerimpl &listenerArg)
		: dir(dirArg), repo(repoArg), listener(listenerArg)
	{
	}

	~reloadNotifyObj() {}


	void installed(const std::string &objname,
		       const x::ptr<x::obj> &lock)

	{
		if (objname == rootcerts)
			reload();
	}

	void reload()
	{
		std::string n=rootcerts;

		LOG_INFO("Reloading root certificates");

		tobjrepoObj::values_t values;
		std::set<std::string> objnames;
		std::set<std::string> notfound;

		objnames.insert(n);

		repo->values(objnames, true, values, notfound);

		tobjrepoObj::values_t::iterator iter=values.find(n);

		if (iter == values.end())
		{
			LOG_FATAL("Root certificates deleted from the"
				  " object repository: aiee!");
			return;
		}

		char buf[repo->obj_name_len(n)];

		repo->obj_name_create(n, buf);

		std::string t=repomg::rootcerts(dir), ttmp=t + ".tmp";

		unlink(ttmp.c_str());

		if (link(buf, ttmp.c_str()) < 0 ||
		    rename(ttmp.c_str(), t.c_str()) < 0)
		{
			LOG_FATAL("Cannot install " << t << ": "
				  << strerror(errno));
			return;
		}

		listener->reload();
	}


	void removed(const std::string &objname,
		     const x::ptr<x::obj> &lock)
	{
	}
};

node::node()
{
}

node::~node()
{
}

node::node(const node &)=default;

node &node::operator=(const node &)=default;

node::node(const std::string &dir)
	: repo(tobjrepo::create(dir)),
	  trackerimpl(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create()),
	  tracker(trackerimpl->getTracker()),
	  listener(clusterlistenerimpl::create(dir)),
	  distributor(x::ptr<trandistributorObj>::create()),
	  repocluster(repoclusterinfoimpl::create(listener->nodeName(),
						  listener->clusterName(),
						  repo,
						  distributor, tracker)),
	  reconnecter_thread(x::ref<reconnecter>::create("reconnect(" +
							 listener->nodeName() +
							 ")")),
	  certnotifier(x::ref<reloadNotifyObj>::create(dir, repo, listener))
{
	tobjrepoObj::values_t values;

	{
		std::set<std::string> objnames;
		std::set<std::string> notfound;

		objnames.insert(rootcerts);

		repo->values(objnames, true, values, notfound);
	}

	std::string rootcerts_filename=repomg::rootcerts(dir);

	tobjrepoObj::values_t::iterator iter=values.find(rootcerts);

	bool reload_flag=false;

	if (iter == values.end())
	{
		reload_flag=true;
	}
	else
	{
		struct stat st1=*iter->second.second->stat(),
			st2=*x::fileattr::create(rootcerts_filename)->stat();

		if (st1.st_dev != st2.st_dev ||
		    st1.st_ino != st2.st_ino)
			reload_flag=true;
	}

	if (reload_flag)
	{
		LOG_INFO("Installing updated root certificates");

		newtran tr=repo->newtransaction();

		std::string nodename=listener->nodeName();

		tr->getOptions()[tobjrepoObj::node_opt]=nodename;

		{
			x::fd fd=tr->newobj(rootcerts);

			fd->write(x::fd::base::open(rootcerts_filename, O_RDONLY));
			fd->close();
		}

		x::uuid uuid=tr->finalize();
		repo->begin_commit(uuid, x::eventfd::create())->commit();
		repo->cancel(uuid);

		std::set<std::string> objnames;
		std::set<std::string> notfound;

		objnames.insert(rootcerts);

		repo->values(objnames, true, values, notfound);

		iter=values.find(rootcerts);

		if (iter == values.end())
			throw EXCEPTION("Unable to find "
					+ std::string(rootcerts) +
					" after installing it?");
		certnotifier->reload();
	}

	repo->installNotifier(certnotifier);
}

class node::distributorMonitorObj : virtual public x::obj {

public:
	x::weakptr<repoclusterinfoimplptr> cluster;

	distributorMonitorObj(const repoclusterinfoimpl &clusterArg)
		: cluster(clusterArg)
	{
	}

	~distributorMonitorObj()
	{
		repoclusterinfoimplptr ptr(cluster.getptr());

		if (!ptr.null())
			ptr->stop();
	}
};

class node::reconnecter::job : public x::timertaskObj {

	clusterlistenerimpl listener;
	clusterinfo cluster;

public:
	job(const clusterlistenerimpl &listenerArg,
	    const clusterinfo &clusterArg);
	~job();
	void run();
};

node::reconnecter::job::job(const clusterlistenerimpl &listenerArg,
			    const clusterinfo &clusterArg)
	: listener(listenerArg), cluster(clusterArg)
{
}

node::reconnecter::job::~job()
{
}

void node::reconnecter::job::run()
{
	LOG_DEBUG("Checking for connections on " << cluster->getthisnodename());
	listener->connectpeers();
	cluster->pingallpeers(x::ptr<x::obj>());
}

class node::certcheck_job : public x::timertaskObj {

	clusterlistenerimpl listener;

public:
	certcheck_job(const clusterlistenerimpl &listenerArg);
	~certcheck_job();
	void run();
};

node::certcheck_job::certcheck_job(const clusterlistenerimpl &listenerArg)
	: listener(listenerArg)
{
}

node::certcheck_job::~certcheck_job()
{
}

void node::certcheck_job::run()
{
	listener->checkcert();
}

void node::start(bool noreconnecter)
{
	// Need to construct the queue for trandistributor first, because
	// repocluster pokes the distributor

	auto msgqueue = trandistributorObj::msgqueue_obj::create(distributor);

	repocluster->initialize();

	tracker->start_threadmsgdispatcher(x::ref<trandistributorObj>(distributor),
			      msgqueue,
			      repocluster, repo,
			      x::ptr<distributorMonitorObj>::create(repocluster));
	tracker->start_threadmsgdispatcher(clusterlistenerimpl(listener),
			      tracker,
			      distributor,
			      repo,
			      x::ptr<clustertlsconnectshutdownObj>::create(),
			      repocluster);

	if (noreconnecter)
		return;

	start_reconnecter();
}

void node::start_reconnecter()
{
	auto job=x::ref<reconnecter::job>::create(listener, repocluster);

	job->run();

	reconnecter_thread->timer->
		scheduleAtFixedRate(job, "reconnect", std::chrono::seconds(60),
				    x::locale::create(""));

	auto chkjob=x::ref<certcheck_job>::create(listener);

	chkjob->run();

	reconnecter_thread->timer->scheduleAtFixedRate(chkjob,
						       "certcheck::interval",
						       std::chrono::hours(12),
						       x::locale::create(""));
}

class node::stop_cb : virtual public x::obj {

public:
	stop_cb() {}
	~stop_cb() {}

	void destroyed()
	{
	}
};

void node::wait()
{
	repocluster->wait();
	trackerimpl->stop_threads();

	// Attach a destructor callback to the repo, distributor, and listener
	// objects

	x::ptr<stop_cb> cb(x::ptr<stop_cb>::create());

	repo->ondestroy([cb]{cb->destroyed();});
	distributor->ondestroy([cb]{cb->destroyed();});
	listener->ondestroy([cb]{cb->destroyed();});

	x::fdptr stopfd=repocluster->getstopfd();

	*this=node();

	// Now, make the destructor callback itself a mcguffin, so when it
	// goes out of scope, we know that all of these three did.

	x::destroy_callback dcf=x::destroy_callback::create();

	cb->ondestroy([dcf]{dcf->destroyed();});
	cb=x::ptr<stop_cb>();
	dcf->wait();
}

class node::quorumcbObj:
	public STASHER_NAMESPACE::repoclusterquorumcallbackbaseObj {

public:
	typedef x::mpcobj<STASHER_NAMESPACE::quorumstate> flag_t;

	flag_t flag;

	quorumcbObj() : flag(false, false)
	{
	}

	~quorumcbObj()
	{
	}

	void quorum(const STASHER_NAMESPACE::quorumstate &state)
	{
		flag_t::lock lock(flag);

		*lock=state;
		lock.notify_all();
	}

	void wait4full(bool forwhat)
	{
		flag_t::lock lock(flag);

		while (lock->full != forwhat)
			lock.wait();
	}

	void wait4majority(bool forwhat)
	{
		flag_t::lock lock(flag);

		while (lock->majority != forwhat)
			lock.wait();
	}
};

void node::debugWaitFullQuorumStatus(bool flag)
{
	x::ref<quorumcbObj> quorumstatus=x::ref<quorumcbObj>::create();

	repocluster->installQuorumNotification(quorumstatus);

	(void)repocluster->debug_inquorum();
	quorumstatus->wait4full(flag);
}

void node::debugWaitMajorityQuorumStatus(bool flag)
{
	x::ref<quorumcbObj> quorumstatus=x::ref<quorumcbObj>::create();

	repocluster->installQuorumNotification(quorumstatus);

	(void)repocluster->debug_inquorum();
	quorumstatus->wait4majority(flag);
}

class node::wait4allconnections : public clusternotifierObj {

public:

	x::mpcobj<bool> allpresent;

	wait4allconnections() : allpresent(false) {}
	~wait4allconnections() {}

        void clusterupdated(const clusterinfoObj::cluster_t &newStatus)

	{
		for (clusterinfoObj::cluster_t::const_iterator
			     b=newStatus.begin(), e=newStatus.end(); b != e;
		     ++b)
		{
			if (b->second.connection.getptr().null())
			{
				LOG_DEBUG("Connection with "
					  << b->first
					  << " is missing");
				return;
			}
			LOG_DEBUG("Connection with "
				  << b->first
				  << " is present");
		}

		x::mpcobj<bool>::lock lock(allpresent);

		*lock=true;
		lock.notify_all();
	}

	void wait()
	{
		x::mpcobj<bool>::lock lock(allpresent);

		while (!*lock)
			lock.wait();
	}
};

void node::debugWait4AllConnections()
{
	x::ref<wait4allconnections> w(x::ref<wait4allconnections>::create());

	repocluster->installnotifycluster(w);
	w->wait();
}

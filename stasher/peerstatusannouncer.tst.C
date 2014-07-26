/*
** Copyright 2012-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"

#include <x/options.H>

#include "peerstatusannouncer.H"
#include "clusterlistener.H"
#include "clusterconnecter.H"
#include "stoppablethreadtracker.H"

LOG_FUNC_SCOPE_DECL(foo,foo);

class myListener;

class mydistributorObj : public trandistributorObj {

public:
	mydistributorObj() throw(x::exception) {}
	~mydistributorObj() throw() {}
	void internal_transaction(const x::ref<internalTransactionObj>
				  &tran) {}
};

class testpeerstatusannouncerObj
	: public peerstatusannouncerObj {

	x::ptr<myListener> listener;

public:

	using peerstatusannouncerObj::install;
	using peerstatusannouncerObj::statusupdated;

	std::string getName() const noexcept
	{
		return peername;
	}

	testpeerstatusannouncerObj(const x::ptr<myListener> &listenerArg,
				   const std::string &peername);
	~testpeerstatusannouncerObj() noexcept;
	void run(const x::fdbase &transport,
		 const x::fd::base::inputiter &inputiter,
		 const STASHER_NAMESPACE::stoppableThreadTracker &tracker,
		 const x::ptr<x::obj> &mcguffin);

	void deserialized(const nodeclusterstatus &newStatus);

	//! The actual main loop

	MAINLOOP_DECL;
};

MAINLOOP_IMPL(testpeerstatusannouncerObj)

typedef x::ptr<testpeerstatusannouncerObj> testpeerannouncer;

class myConnecter : public clusterconnecterObj {

	STASHER_NAMESPACE::stoppableThreadTracker tracker;
	clusterinfo cluster;
	x::ptr<myListener> listener;

public:
	myConnecter(const std::string &threadname,
		    const STASHER_NAMESPACE::stoppableThreadTracker &trackerArg,
		    const clusterinfo &clusterArg,
		    const x::ptr<myListener> &listenerArg);

	~myConnecter() noexcept;

	void connected(const std::string &peername,
		       const x::fd &socket,
		       const x::gnutls::sessionptr &session,
		       const x::fd::base::inputiter &inputiter,
		       const nodeclusterstatus &peerstatus,
		       time_t timestamp,
		       const x::uuid &connuuid,
		       const clusterlistener &listener);
};

class myListener : public clusterlistenerObj {

	STASHER_NAMESPACE::stoppableThreadTracker tracker;

public:
	clusterinfoptr cluster;

	std::list<nodeclusterstatus> recvlog;
	std::mutex mutex;
	std::condition_variable cond;
	tobjrepo repo;

	myListener(const std::string &directoryArg,
		   const STASHER_NAMESPACE::stoppableThreadTracker &trackerArg,
		   const tobjrepo &repoArg);

	~myListener() noexcept;

	void start_network(const x::fd &sock,
			   const x::sockaddr &addr)
;
	void start_privsock(const x::fd &sock,
			    const x::sockaddr &addr)
;

	void start_pubsock(const x::fd &sock,
			   const x::sockaddr &addr)
;
};

testpeerstatusannouncerObj
::testpeerstatusannouncerObj(const x::ptr<myListener> &listenerArg,
			     const std::string &peername)
 : peerstatusannouncerObj(peername),
			      listener(listenerArg)
{
}

testpeerstatusannouncerObj::~testpeerstatusannouncerObj() noexcept
{
}

void testpeerstatusannouncerObj::run(const x::fdbase &transport,
				     const x::fd::base::inputiter &inputiter,
				     const STASHER_NAMESPACE::
				     stoppableThreadTracker &tracker,
				     const x::ptr<x::obj> &mcguffin)

{
	mainloop(transport, inputiter, tracker, mcguffin);
}

void testpeerstatusannouncerObj::deserialized(const nodeclusterstatus
					      &newStatus)

{
	std::lock_guard<std::mutex> lock(listener->mutex);

	listener->recvlog.push_back(newStatus);

	peerstatusannouncerObj::deserialized(newStatus);

	listener->cond.notify_all();
}


myConnecter::myConnecter(const std::string &threadname,
			 const STASHER_NAMESPACE::stoppableThreadTracker &trackerArg,
			 const clusterinfo &clusterArg,
			 const x::ptr<myListener> &listenerArg)

	: clusterconnecterObj(threadname),
	  tracker(trackerArg),
	  cluster(clusterArg),
	  listener(listenerArg)
{
}

myConnecter::~myConnecter() noexcept
{
}

void myConnecter::connected(const std::string &peername,
			    const x::fd &socket,
			    const x::gnutls::sessionptr &session,
			    const x::fd::base::inputiter &inputiter,
			    const nodeclusterstatus &peerstatus,
			    time_t timestamp,
			    const x::uuid &connuuid,
			    const clusterlistener &listener)

{
	auto testthread=
		x::ref<testpeerstatusannouncerObj>::create(listener, peername);

	x::ptr<x::obj> mcguffin(x::ptr<x::obj>::create());

	if (!testthread->install(cluster).first)
	{
		std::cerr << "install(" << peername << ") failed"
			  << std::endl;
		return;
	}

	tracker->start(testthread, socket, inputiter, tracker, mcguffin);
	std::cerr << "Started connection with " << peername
		  << std::endl;
}

myListener::myListener(const std::string &directoryArg,
		       const STASHER_NAMESPACE::stoppableThreadTracker &trackerArg,
		       const tobjrepo &repoArg)
	: clusterlistenerObj(directoryArg),
	  tracker(trackerArg),
	  repo(repoArg)
{
}

myListener::~myListener() noexcept
{
}

void myListener::start_network(const x::fd &sock,
			       const x::sockaddr &addr)

{
	auto conn=x::ref<myConnecter>::create("incoming connection",
					      tracker, cluster,
					      x::ptr<myListener>(this));

	tracker->start(conn, cluster,
		       x::ptr<myListener>(this), sock, repo,
		       x::ptr<mydistributorObj>::create());
}

void myListener::start_privsock(const x::fd &sock,
				const x::sockaddr &addr)

{
}

void myListener::start_pubsock(const x::fd &sock,
			       const x::sockaddr &addr)

{
}

class connection_factory : public clusterinfoObj::connectpeers_callback {

	STASHER_NAMESPACE::stoppableThreadTracker tracker;
	clusterinfo cluster;
	x::ptr<myListener> listener;
	tobjrepo repo;

public:
	connection_factory(const STASHER_NAMESPACE::stoppableThreadTracker &trackerArg,
			   const clusterinfo &clusterArg,
			   const x::ptr<myListener> &listenerArg,
			   const tobjrepo &repoArg)
		: tracker(trackerArg),
		  cluster(clusterArg),
		  listener(listenerArg),
		  repo(repoArg)
	{
	}

	x::ptr<x::obj> operator()(const std::string &peername)
		const
	{
		auto connecter=x::ref<myConnecter>::create("connect: " +
							   peername,
							   tracker,
							   cluster,
							   listener);

		tracker->start(connecter, cluster, listener, peername,
			       repo,
			       x::ptr<mydistributorObj>::create());

		return connecter;
	}
};

class tnode {

public:
	STASHER_NAMESPACE::stoppableThreadTrackerImpl trackerimpl;
	STASHER_NAMESPACE::stoppableThreadTracker tracker;
	tobjrepo repo;
	x::ref<myListener> listener;
	clusterinfo cluster;
	std::list<nodeclusterstatus> recvlog;

	tnode(const std::string &dirname,
	      const STASHER_NAMESPACE::nodeinfomap &clusterinfomap)

		: trackerimpl(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create()),
		  tracker(trackerimpl->getTracker()),
		  repo(tobjrepo::create(dirname + "/data")),
		  listener(x::ref<myListener>::create(dirname, tracker, repo)),
		  cluster(clusterinfo::create(listener->nodeName(),
					      listener->clusterName(),
					      tracker, clusterinfomap))
	{
		listener->cluster=cluster;
		tracker->start(listener);
	}

	void connectpeers()
	{
		cluster->connectpeers(connection_factory(tracker,
							 cluster,
							 listener,
							 repo));
	}
};

class reporter : public clusterstatusnotifierObj {

public:

	std::string name;

	std::mutex mutex;
	std::condition_variable cond;

	nodeclusterstatus status;
	bool flag;

	reporter(const std::string &nameArg)
		: name(nameArg), flag(false)
	{
	}

	~reporter() noexcept
	{
	}

	void statusupdated(const nodeclusterstatus &newStatus)

	{
		std::lock_guard<std::mutex> lock(mutex);

		status=newStatus;
		flag=true;
		cond.notify_all();

		std::cout << name << ": ";
		newStatus.toString(std::cout);
		std::cout << std::endl;
	}
};

void test1()
{
	STASHER_NAMESPACE::nodeinfomap clusterinfomap;

	x::ptr<reporter> repa(x::ptr<reporter>
			      ::create(tstnodes::getnodefullname(0))),
		repb(x::ptr<reporter>::create(tstnodes::getnodefullname(1)));

	clusterinfomap[tstnodes::getnodefullname(0)].options
		.insert(std::make_pair("HOST", "localhost"));

	clusterinfomap[tstnodes::getnodefullname(1)].options=
		clusterinfomap[tstnodes::getnodefullname(0)].options;

	tnode b(tstnodes::getnodedir(1), clusterinfomap);

	b.cluster->installnotifyclusterstatus(repb);

	tnode a(tstnodes::getnodedir(0), clusterinfomap);

	a.cluster->installnotifyclusterstatus(repa);

	a.connectpeers();

	{
		std::unique_lock<std::mutex> lock(repb->mutex);

		while (repb->status.master != tstnodes::getnodefullname(1) ||
		       repb->status.slavecount != 1)
			repb->cond.wait(lock);
	}

	{
		std::unique_lock<std::mutex> lock(repa->mutex);

		while (repa->status.master != tstnodes::getnodefullname(1) ||
		       repa->status.slavecount != 1)
			repa->cond.wait(lock);
	}

	repb=repa=x::ptr<reporter>();

	std::cout << "Waiting for expected status update list on node b"
		  << std::endl;

	{
		std::unique_lock<std::mutex> lock(b.listener->mutex);

		while (b.listener->recvlog.size() != 3)
			b.listener->cond.wait(lock);
	}

	std::cout << "Waiting for expected status update list on node a"
		  << std::endl;

	{
		std::unique_lock<std::mutex> lock(a.listener->mutex);

		while (a.listener->recvlog.size() != 2)
			a.listener->cond.wait(lock);
	}

	a.trackerimpl->stop_threads();
	b.trackerimpl->stop_threads();

	std::ostringstream ao;

	std::cout << std::endl;
	for (std::list<nodeclusterstatus>::iterator
		     rb(b.listener->recvlog.begin()),
		     re(b.listener->recvlog.end()); rb != re; ++rb)
	{
		ao << rb->master << '/' << rb->slavecount << '\n';

		std::cout << tstnodes::getnodefullname(0) << "->" << tstnodes::getnodefullname(1)
			  << ": ";

		rb->toString(std::cout);
		std::cout << std::endl;
	}

	std::ostringstream bo;

	std::cout << std::endl;
	for (std::list<nodeclusterstatus>::iterator
		     rb(a.listener->recvlog.begin()),
		     re(a.listener->recvlog.end()); rb != re; ++rb)
	{
		bo << rb->master << '/' << rb->slavecount << '\n';

		std::cout << tstnodes::getnodefullname(1) << "->" << tstnodes::getnodefullname(0)
			  << ": ";

		rb->toString(std::cout);
		std::cout << std::endl;
	}

	std::string namea=tstnodes::getnodefullname(0);
	std::string nameb=tstnodes::getnodefullname(1);

	if (ao.str() !=
	    namea + "/0\n" + // [INITIALSTATUS]
	    nameb + "/0\n" + // [UPDATEDSTATUS]
	    nameb + "/1\n" || // [RECVUPDATE]
	    bo.str() !=
	    nameb + "/0\n" +
	    nameb + "/1\n")
		throw EXCEPTION("Node status update didn't go as planned:\n"
			        + ao.str() + "\n"
				+ bo.str() + "\n");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	tstnodes nodes(2);

	ALARM(30);

	try {
		test1();
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

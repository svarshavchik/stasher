/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "clusterlistenerimpl.H"
#include "tobjrepo.H"
#include "nsview.H"
#include "stoppablethreadtracker.H"
#include "spacemonitor.H"
#include "clusterconnecterimpl.H"
#include "trandistributor.H"
#include "clustertlsconnectshutdown.H"
#include "clusterstatusnotifier.H"
#include "localconnection.H"
#include "localprivconnection.H"
#include "repoclusterinfoimpl.H"
#include "nslist.H"
#include "nsview.H"
#include "localstatedir.h"

#include <x/timespec.H>
#include <x/pwd.H>
#include <x/grp.H>

LOG_CLASS_INIT(clusterlistenerimplObj);

x::property::value<size_t>
clusterlistenerimplObj::maxgetobjects("connection::maxgetobjects", 100);

x::property::value<bool>
clusterlistenerimplObj::debugnonewconnect("connection::debugnonewconnect", false);

x::property::value<std::string>
clusterlistenerimplObj::appsdir("appsdir", localstatedir "/stasher/apps");

clusterlistenerimplObj
::clusterlistenerimplObj(const std::string &directoryArg)
	: clusterlistenerObj(directoryArg),
	  spacedf(spacemonitor::create(x::df::create(directoryArg))),
	  getsemaphore(x::semaphore::create(maxgetobjects))
{
}

clusterlistenerimplObj::~clusterlistenerimplObj() noexcept
{
}

class clusterlistenerimplObj::margin_callback : public clusternotifierObj {

public:
	x::weakptr<clusterlistenerimplptr> listener;

	margin_callback(const clusterlistenerimplptr &listenerArg)
		: listener(listenerArg)
	{
	}

	~margin_callback() noexcept
	{
	}

        void clusterupdated(const clusterinfoObj::cluster_t &newStatus)

	{
		clusterlistenerimplptr ptr(listener.getptr());

		if (!ptr.null())
		{
			ptr->update_reserved_space(margin_message
						   (newStatus.size()));

			ptr->connectnewpeers();
		}
	}
};

void clusterlistenerimplObj::connectnewpeers()
{
	if (!debugnonewconnect.getValue())
		connectpeers();
}

void clusterlistenerimplObj::run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
				 const STASHER_NAMESPACE::stoppableThreadTracker
				 &trackerArg,
				 const x::ptr<trandistributorObj> &distributorArg,
				 const tobjrepo &distrepoArg,
				 const x::ptr<clustertlsconnectshutdownObj> &shutdownArg,
				 const repoclusterinfoimpl &clusterArg)
{
	msgqueue_auto msgqueue(this);

	threadmsgdispatcher_mcguffin=x::ptr<x::obj>();

	tracker= &trackerArg;
	distributor= &distributorArg;
	distrepo= &distrepoArg;
	shutdown= &shutdownArg;
	cluster= &clusterArg;

	npeers=1;

	spacemonitorObj::reservationptr reservation_margin_ref;

	reservation_margin= &reservation_margin_ref;

	auto margin_callbackRef=
		x::ref<margin_callback>::create(clusterlistenerimplptr(this));

	clusterArg->installnotifycluster(margin_callbackRef);

	clusterlistenerObj::run(msgqueue);
}

void clusterlistenerimplObj::start_network(const x::fd &sock,
					   const x::sockaddr &addr)
{
	LOG_DEBUG(nodeName() << ": connection from " << addr->address());

	auto connecter=x::ref<clusterconnecterimplObj>
		::create((std::string)*addr,
			 *cluster,
			 *tracker,
			 *shutdown,
			 *distributor,
			 *distrepo,
			 spacedf);

	(*tracker)->start(connecter,
			  *cluster,
			  clusterlistener(this),
			  sock, *distrepo,
			  *distributor);
}

// ----------------------------------------------------------------------------

class clusterlistenerimplObj::connectpeers_cb
	: public clusterinfoObj::connectpeers_callback {

public:
	clusterinfo cluster;
	STASHER_NAMESPACE::stoppableThreadTracker tracker;
	clusterlistener listener;
	x::ptr<clustertlsconnectshutdownObj> shutdown;
	x::ptr<trandistributorObj> distributor;
	tobjrepo distrepo;
	spacemonitor spacedf;

	connectpeers_cb(const clusterinfo &clusterArg,
			const STASHER_NAMESPACE::stoppableThreadTracker &trackerArg,
			const clusterlistener &listenerArg,
			const x::ptr<clustertlsconnectshutdownObj> &shutdownArg,
			const x::ptr<trandistributorObj> &distributorArg,
			const tobjrepo &distrepoArg,
			const spacemonitor &spacemonitorArg
			);

	~connectpeers_cb() noexcept;

	x::ptr<x::obj> operator()(const std::string &peername) const;
};

clusterlistenerimplObj::connectpeers_cb
::connectpeers_cb(const clusterinfo &clusterArg,
		  const STASHER_NAMESPACE::stoppableThreadTracker &trackerArg,
		  const clusterlistener &listenerArg,
		  const x::ptr<clustertlsconnectshutdownObj> &shutdownArg,
		  const x::ptr<trandistributorObj> &distributorArg,
		  const tobjrepo &distrepoArg,
		  const spacemonitor &spacemonitorArg)
	: cluster(clusterArg), tracker(trackerArg),
	  listener(listenerArg), shutdown(shutdownArg),
	  distributor(distributorArg),
	  distrepo(distrepoArg),
	  spacedf(spacemonitorArg)
{
}

clusterlistenerimplObj::connectpeers_cb::~connectpeers_cb() noexcept
{
}

x::ptr<x::obj> clusterlistenerimplObj::connectpeers_cb
::operator()(const std::string &peername) const
{
	x::ref<clusterconnecterimplObj>
		connecter(x::ref<clusterconnecterimplObj>::create
			  (peername, cluster, tracker,
			   shutdown, distributor, distrepo, spacedf));

	tracker->start(connecter,
		       cluster,
		       listener,
		       peername,
		       distrepo,
		       distributor);
	return connecter;
}

void clusterlistenerimplObj::dispatch_connectpeers()
{
	LOG_DEBUG(nodeName() << ": contacting peers");
	(*cluster)
		->connectpeers(connectpeers_cb(*cluster,
					       *tracker,
					       clusterlistener(this),
					       *shutdown,
					       *distributor,
					       *distrepo,
					       spacedf));
}

// --------------------------------------------------------------------------

class clusterlistenerimplObj::retr_credentialsObj
	: public x::eventqueuemsgdispatcherObj {

	LOG_CLASS_SCOPE;

public:
	static x::property::value<time_t> timeout_interval;

	x::fd sock;
	x::weakptr<clusterlistenerimplptr> listener;
	void (clusterlistenerimplObj::*start_conn)(const x::fd &,
						   const nsmap::clientcred &);

	retr_credentialsObj(const x::fd &sockArg,
			    const clusterlistenerimplptr &listenerArg,
			    void (clusterlistenerimplObj::*start_connArg)
			    (const x::fd &, const nsmap::clientcred &));
	~retr_credentialsObj() noexcept;

	void run();
};

LOG_CLASS_INIT(clusterlistenerimplObj::retr_credentialsObj);

void clusterlistenerimplObj::start_privsock(const x::fd &sock,
					    const x::sockaddr &addr)
{
	LOG_DEBUG(nodeName() << ": privileged local connection");

	start_credentials(sock, &clusterlistenerimplObj::start_privlocalconn);
}

void clusterlistenerimplObj::start_pubsock(const x::fd &sock,
					   const x::sockaddr &addr)
{
	LOG_DEBUG(nodeName() << ": local connection");

	start_credentials(sock, &clusterlistenerimplObj::start_localconn);
}

void clusterlistenerimplObj::
start_credentials(const x::fd &sock,
		  void (clusterlistenerimplObj::*start_connArg)
		  (const x::fd &, const nsmap::clientcred &))
{
	(*tracker)->start(x::ref<retr_credentialsObj>
			  ::create(sock, clusterlistenerimplptr(this),
				   start_connArg));
}

void clusterlistenerimplObj::dispatch_start_localconn(const x::fd &sock,
						      const nsmap::clientcred &fromwho)
{
	std::string name=fromwho;

	std::string host=nodeName();
	std::string clustersuffix="." + clusterName();

	if (host.size() > clustersuffix.size() &&
	    host.substr(host.size()-clustersuffix.size()) == clustersuffix)
		host=host.substr(0, host.size()-clustersuffix.size());

	std::map<std::string, std::string> rwnamespaces,
		ronamespaces;

	x::fdptr mapping;

	{
		nsmap::local_map_t localmap;

		try {
			nsmap::get_local_map(appsdir.getValue(), localmap);
		} catch (const x::exception &e)
		{
			// Ignore errors during regression tests
			LOG_WARNING(e);
		}

		nsmap::local_map_t::iterator localmap_end_iter=localmap.end(),
			localmap_iter=localmap_end_iter;

		try {
			auto s=x::fileattr::create(fromwho.path)->stat();

			localmap_iter=localmap.find(std::make_pair(s->st_dev,
								   s->st_ino));
		} catch (const x::exception &e)
		{
			// Ignore if client's path is inaccessible, too bad.
			LOG_WARNING(e);
		}

		if (localmap_iter != localmap_end_iter)
		{
			rwnamespaces=localmap_iter->second.first;
			ronamespaces=localmap_iter->second.second;
		}
		else
		{
			std::set<std::string> objects;
			std::set<std::string> notfound;

			objects.insert(NSLISTOBJECT);

			tobjrepoObj::values_t values;

			(*distrepo)->values(objects, true, values, notfound);

			tobjrepoObj::values_t::iterator p=
				values.find(NSLISTOBJECT);

			if (p != values.end())
				mapping=p->second.second;
		}
	}

	if (!mapping.null())
	{
		x::fdinputiter beg_iter(mapping), end_iter(mapping);

		x::deserialize::iterator<x::fd::base::inputiter>
			deserialize_iter(beg_iter, end_iter);

		nslist map;

		deserialize_iter(map);
		rwnamespaces=fromwho.computemappings(map.rw, host);
		ronamespaces=fromwho.computemappings(map.ro, host);
	}

	nsview namespaceview=nsview::create(ronamespaces, rwnamespaces,
					    "sandbox");

	LOG_DEBUG(nodeName() << ": user connection from " << name
		  << ":" << std::endl
		  << ({
				  std::ostringstream o;

				  namespaceview->toString(o);

				  o.str();
			  }));

	start_conn(x::ref<localconnectionObj>
		   ::create(name,
			    namespaceview,
			    *distrepo,
			    *tracker,
			    *distributor,
			    *cluster,
			    spacedf,
			    getsemaphore),
		   sock,
		   x::fd::base::inputiter(sock),
		   x::ptr<x::obj>());
}

void clusterlistenerimplObj::dispatch_start_privlocalconn(const x::fd &sock,
							  const nsmap::clientcred &fromwho)
{
	std::string name=fromwho;

	LOG_DEBUG(nodeName() << ": admin connection from " << name);

	start_conn(x::ref<localprivconnectionObj>
		   ::create(name,
			    *distrepo,
			    *tracker,
			    *distributor,
			    *cluster,
			    spacedf,
			    getsemaphore,
			    clusterlistener(this)),
		   sock,
		   x::fd::base::inputiter(sock),
		   x::ptr<x::obj>());
}

void clusterlistenerimplObj::start_conn(const x::ref<localconnectionObj> &conn,
					const x::fd &transport,
					const x::fd::base::inputiter &inputiter,
					const x::ptr<x::obj> &mcguffin)
{
	// We must start the new thread first. start_thread() does not return
	// until the thread starts and creates its message queue.
	//
	// installnotifycluster() and installnotifyclusterstatus() send
	// the current status immediately, so the thread's message queue
	// must exist by that time.

	(*tracker)->start_thread(conn, transport, inputiter, *tracker,
				 mcguffin);
	(*cluster)->installnotifyclusterstatus(conn);
	(*cluster)->installnotifycluster(conn);
}


x::property::value<time_t> clusterlistenerimplObj::retr_credentialsObj
::timeout_interval("localcredtimeout", 15);

clusterlistenerimplObj::retr_credentialsObj::
retr_credentialsObj(const x::fd &sockArg,
		    const clusterlistenerimplptr &listenerArg,
		    void (clusterlistenerimplObj::*start_connArg)
		    (const x::fd &, const nsmap::clientcred &))
	: sock(sockArg), listener(listenerArg),
	  start_conn(start_connArg)
{
}

clusterlistenerimplObj::retr_credentialsObj::~retr_credentialsObj() noexcept
{
}

void clusterlistenerimplObj::retr_credentialsObj::run()
{
	try {
		sock->nonblock(true);
		sock->recv_credentials(true);

		struct pollfd pfd[2];

		pfd[0].fd=sock->getFd();
		pfd[1].fd=msgqueue->getEventfd()->getFd();
		pfd[0].events=POLLIN;
		pfd[1].events=POLLIN;

		x::timespec now=x::timespec::getclock(CLOCK_MONOTONIC),
			timeout=now;

		timeout.tv_sec += timeout_interval.getValue();

		while (now < timeout)
		{
			while (!msgqueue->empty())
				msgqueue->pop()->dispatch();

			int rc=::poll(pfd, 2, (timeout-now).tv_sec * 1000);

			now=x::timespec::getclock(CLOCK_MONOTONIC);

			if (rc < 0)
				continue;
			if (pfd[0].revents & POLLIN)
			{
				x::fd::base::credentials_t uc;

				sock->recv_credentials(uc);

				clusterlistenerimplptr ptr=listener.getptr();

				if (ptr.null())
					return;

				nsmap::clientcred
					info(x::httportmap::create()->
					     pid2exe(uc.pid),
					     x::passwd(uc.uid)->pw_name,
					     x::group(uc.gid)->gr_name);

				if (info.path.size() == 0)
				{
					LOG_ERROR("Connection from pid "
						  << uc.pid
						  << " ("
						  << info.username << ":"
						  << info.groupname
						  << "): not registered with "
						  "the portmapper");
					return;
				}

				((&*ptr)->*start_conn)(sock, info);
				return;
			}
		}
	} catch (const x::exception &e)
	{
		LOG_ERROR(e);
	}
}

// --------------------------------------------------------------------------


x::property::value<x::memsize>
clusterlistenerimplObj::reserved_space("reserved::diskspace",
				       x::memsize(1024L * 1024L * 16));
x::property::value<size_t>
clusterlistenerimplObj::reserved_inodes("reserved::inodes", 100);

void clusterlistenerimplObj::dispatch_update_reserved_space(const margin_message &margin)
{
	if (margin.set_new_peers)
		npeers=margin.npeers;

	*reservation_margin=spacemonitorObj::reservationptr();
	// Release space first

	*reservation_margin=spacedf->
		reservespace_alloc(spacedf->calculate_alloc
				   (reserved_space.getValue().bytes)
				   + npeers,
				   reserved_inodes.getValue()
				   + npeers * 5);
}

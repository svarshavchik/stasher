/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "objrepo.H"
#include "stasher/client.H"
#include "stasher/puttransaction.H"
#include "stasher/objname.H"
#include "stasher/serverstatuscallback.H"
#include "clusterlistener.H"
#include "fdobjrwthread.H"
#include "writtenobj.H"
#include "adminstop.H"
#include "sendupdatesreq.H"
#include "requestmsgs.auto.H"
#include "replymsgs.auto.H"

#include "dirs.H"
#include "dirmsgreply.H"
#include "stasher/userput.H"
#include "stasher/userhelo.H"
#include "userputreply.H"
#include "usergetuuids.H"
#include "usergetuuidsreply.H"
#include "userchanged.H"
#include "localstatedir.h"

#include <x/threads/run.H>
#include <x/property_properties.H>
#include <x/timerfd.H>
#include <x/fdtimeoutconfig.H>
#include <x/netaddr.H>
#include <x/destroy_callback.H>
#include <x/weaklist.H>

STASHER_NAMESPACE_START

class LIBCXX_HIDDEN my_portmapperObj : virtual public x::obj {

 public:
	x::httportmap portmapper;

	my_portmapperObj() : portmapper(x::httportmap::base::regpid2exe())
	{
	}

	~my_portmapperObj() {
	}
};

x::singleton<my_portmapperObj> myportmapper LIBCXX_HIDDEN;

x::httportmap clientObj::get_portmapper()
{
	return myportmapper.get()->portmapper;
}

clientBase::connstatusObj::connstatusObj()
	: result(req_disconnected_stat)
{
}

clientBase::connstatusObj::~connstatusObj()
{
}

void invalid_object_name()
{
	throw EXCEPTION("Invalid object name");
}

const char clientBase::clusterconfigobj[]="etc/cluster";
const char clientBase::maxobjects[]="etc/maxobjects";
const char clientBase::maxobjectsize[]="etc/maxobjectsize";

const char clientObj::lost_connection_errmsg[]="Lost connection to the server";

class LIBCXX_HIDDEN clientObj::implObj : public fdobjrwthreadObj  {

	LOG_CLASS_SCOPE;

	//! Socket
	std::string sockname;

	//! Initial message from the server
	userhelo helo;

	//! A pointer to the startup mcguffin on the executing thread's stack
	x::ptr<x::obj> *limits_mcguffin;

public:

	static const char DISCONNECTED_MSG[] LIBCXX_INTERNAL;

	//! Return limits

	class getheloObj : virtual public x::obj, public userhelo {

	public:
		getheloObj() {}
		~getheloObj() {}
	};

	//! Convenience wrapper

	template<typename msg_type>
	class deser : public msg_type {

	public:
		//! Constructor

		template<typename obj_type>
		deser(obj_type &dummy) {}

		//! Destructor
		~deser() {}
	};

	template<typename iter_type>
	static void classlist(iter_type &iter)
	{
		fdobjrwthreadObj::classlist(iter);

#include "client.classlist.auto.H"

		iter.template serialize<userhelo, deser<userhelo> >();
		iter.template serialize<userputreply, deser<userputreply> >();
		iter.template serialize<fdobjwriterthreadObj
					::sendfd_ready>();
		iter.template serialize<usergetuuidsreply,
					deser<usergetuuidsreply> >();
		iter.template serialize<usergetuuidsreply::openobjects,
					deser<usergetuuidsreply::openobjects>
					> ();
		iter.template serialize<dirmsgreply, deser<dirmsgreply> >();
		iter.template serialize<userchanged, deser<userchanged> >();
		iter.template serialize<STASHER_NAMESPACE::clusterstate,
					deser<STASHER_NAMESPACE::clusterstate>
					>();
	}

	implObj(const std::string &socknameArg)
		: fdobjrwthreadObj(socknameArg), sockname(socknameArg),
		received_fd_handler(&implObj::received_fd_unknown)
		{
			readTimeout_value=0;
		}
	~implObj() {}

	// Connect to a server socket, send credentials

	static x::fd connect_socket(const std::string &filename)
		LIBCXX_INTERNAL;

	static x::property::value<time_t> connect_timeout;

	static x::property::value<std::string> default_nodedir;

	// Client service thread

	void run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
		 x::ptr<x::obj> &conn_mcguffin,
		 clientBase::connstatusptr &conn_status,
		 const x::ref<x::obj> &mcguffin);

	// Most requests get associated with a mcguffin, and some response object

	class LIBCXX_INTERNAL reqinfo {
	public:
		x::ptr<x::obj> mcguffin;
		x::ref<x::obj> resp;

		reqinfo(const x::ptr<x::obj> &mcguffinArg,
			const x::ref<x::obj> &respArg)
			: mcguffin(mcguffinArg), resp(respArg)
		{
		}

		~reqinfo()
		{
		}
	};

	// Metadata stored for a usergetuudis response

	class LIBCXX_INTERNAL uuidgetinfoObj : virtual public x::obj {

	public:

		getrequest req;

		std::vector<x::fdptr> filedescs;

		uuidgetinfoObj(const getrequest &reqArg): req(reqArg) {}

		~uuidgetinfoObj() {}
	};

	// Requests and responses get associated with a uuid.
	// This map is held on the executing thread's stack

	typedef std::map<x::uuid, reqinfo> reqs_t;

	reqs_t *reqs;

	MAINLOOP_DECL;

 private:

	//! We'll always accept file descriptors from the server, we trust it
	void accept_fds(size_t n);

	//! Received some file descriptors from the server

	void received_fd(const std::vector<x::fdptr> &fdArg);

	//! Handler for the file descriptors from the server

	void (implObj::*received_fd_handler)(const std::vector<x::fdptr> &);

	//! We don't expect any file descriptors from the server?

	void received_fd_unknown(const std::vector<x::fdptr> &fdArg);

	//! Incoming file descriptors for this get request

	x::uuid fdgetuuid;

	//! Receive file descriptors for a get request
	void received_fd_get(const std::vector<x::fdptr> &fdArg);

	// Queue of get requests that have not been sent to the server yet
	// because the server is already processing the maximum number of
	// get requests. This is on the executing thread's stack

	std::queue<x::ptr<writtenObj<usergetuuids> > > *getqueue;

	//! How many GET requests have been sent to the server
	size_t getpending_cnt;

	// Send a GET request, if the server can take it.

	void sendgetrequests();

	// A container for active subscriptions

	// The value is the subscribe callback, and a mcguffin.
	// Each subscription for an object is registered here, and the mcguffin
	// send the unsubscribe() message.
	//
	// The same object name may have multiple client subscriptions
	// outstanding, hence the multimap. When an unsubscribe() message
	// gets received, it gets removed from the multimap, and if this is
	// the last logical client subscription for this object, an
	// endsub request is sent to the server.

	typedef std::multimap<std::string, std::pair<x::ref<subscriberObj>,
						     x::ref<x::obj> > >
		subscriber_map_t;

	// Active subscriptions, on the executing thread's stack

	subscriber_map_t *subscriber_map;

 public:
	// Subscription mcguffin. The destructor sends an unsubscribe message
	// to the client thread, to remove the subscription.

	class subscriptionMcguffinObj : virtual public x::obj {

	public:
		x::weakptr<x::ptr<implObj> > client;

		subscriber_map_t::iterator iterator;

		subscriptionMcguffinObj() {}
		~subscriptionMcguffinObj()
		{
			x::ptr<implObj> ptr=client.getptr();

			if (!ptr.null())
				ptr->unsubscribe(iterator);
		}
	};
 private:
	class serverstatuscallbackinfo;

	//! Container for the list of subscribers to the server's status
	typedef std::list<serverstatuscallbackinfo> server_status_subscribers_t;

	//! List of subscribers to the server's status
	server_status_subscribers_t *server_status_subscribers;

	class serverstatusSubscriptionMcguffinObj : virtual public obj {

	public:
		serverstatusSubscriptionMcguffinObj() {}

		x::weakptr<x::ptr<implObj> > conn;
		server_status_subscribers_t::iterator subscriber;

		~serverstatusSubscriptionMcguffinObj()
		{

			auto p=conn.getptr();

			if (!p.null())
				p->unsubscribeserverstatus(subscriber);
		}
	};

 public:

#include "client.msgs.H"

 public:
#include "client.deserialized.auto.H"

 private:

	// Information about server status callbacks

	class serverstatuscallbackinfo {

	public:
		serverstatuscallback callback;
		x::ref<x::obj> cancel_mcguffin;

		// The original subscription request+mcguffin
		// Cleared when the server acks our subscription request.

		x::ptr<x::obj> penreq_mcguffin;
		x::ptr<subscribeserverstatusresultsObj> penreq;

		serverstatuscallbackinfo(const serverstatuscallback
					 &callbackArg,
					 const x::ref<x::obj>
					 &cancel_mcguffinArg,

					 const x::ptr<x::obj>
					 &penreq_mcguffinArg,
					 const
					 x::ref<subscribeserverstatusresultsObj>
					 &penreqArg)
			: callback(callbackArg),
			  cancel_mcguffin(cancel_mcguffinArg),
			  penreq_mcguffin(penreq_mcguffinArg),
			  penreq(penreqArg)
		{
		}

		void ack()
		{
			if (!penreq.null())
			{
				penreq->status=req_processed_stat;
				penreq_mcguffin=nullptr;
				penreq=x::ptr<subscribeserverstatusresultsObj>();
			}
		}
	};

	// Add a subscription

	void register_subscription(const std::string &objname,
				   const client::base::subscriber &subscriber,
				   const x::ref<subscriberesultsObj::recvObj> &results,
				   req_stat_t status);

	// A container for a list of pending subscription requests.
	// The value is a list. When a subscription request for an object comes
	// in, a beginsub message is sent to the server. If another subscription
	// request gets received before the response from the server, it gets
	// appended to the list.
	//
	// subscribe_info holds the original parameters to subscribe(),
	// including the request mcguffin. The subscribe() request isn't
	// considered to be processed until the beginsub response is received.
	//
	// We are responsible for sending a subscription request for a
	// particular object name exactly once, even if we get multiple
	// subscription requests for the same object from the clients.

	class subscribe_info {
	public:
		std::string objname;
		client::base::subscriber subscriber;
		x::ref<subscriberesultsObj::recvObj> results;
		x::ptr<x::obj> mcguffin;
	};

	typedef std::map<std::string, std::list<subscribe_info> > subq_t;

	// Pending subscription requests

	subq_t subq;

	// The server subscription requests are IDed by a uuid, and when the
	// server acks it, we need to find the request queue

	std::map<x::uuid, std::string> subquuids;

 public:
	//! Retrieve connection limits

	userinit getlimits()
	{
		return helo.limits;
	}

	void deserialized(const userhelo &msg);
	void deserialized(const userputreply &msg);
	void set_putresults(const putrequest &req,
			    const req_stat_t status,
			    const x::uuid &newuuid=x::uuid());

	void deserialized(const fdobjwriterthreadObj::sendfd_ready
			  &msg);
	void deserialized(const usergetuuidsreply &msg);
	void deserialized(const usergetuuidsreply::openobjects &msg);
	void deserialized(const userchanged &msg);

	using fdobjrwthreadObj::deserialized;

 private:

	//! Whether we asked the server to be subscribed to its status

	bool server_status_subscribed;

	//! Tell the server to subscribe/unsubscribe to its status

	//! \return true if a message to the server was sent.
	//!
	bool update_server_status_subscription();

	//! Last quorum status received from the server
	STASHER_NAMESPACE::clusterstate last_quorum_status_received;

	//! Whether we have it.
	bool have_last_quorum_status_received;

 public:
	void deserialized(const STASHER_NAMESPACE::clusterstate &msg)
;
};

MAINLOOP_IMPL(clientObj::implObj);

LOG_CLASS_INIT(STASHER_NAMESPACE::clientObj::implObj);

const char clientObj::implObj::DISCONNECTED_MSG[]
="Disconnected from the server";

x::property::value<time_t> clientObj::implObj
::connect_timeout("objrepo::client::connect_timeout", 15);

x::property::value<std::string>
clientObj::implObj::default_nodedir("objrepo::nodedir",
				    localstatedir "/stasher/nodes");

clientObj::clientObj(const std::string &socknameArg)
	: sockname(socknameArg), portmap(get_portmapper()),
	  impl(impl_t::create())
{
}

clientObj::~clientObj()
{
	disconnect();
}

clientObj::implptr clientObj::conn()
{
	implptr p=impl->getptr();

	if (p.null())
	{
		std::unique_lock<std::mutex> lock(objmutex);

		p=impl->getptr();

		if (!p.null())
			return p;

		try {
			// Try to reconnect

			return std::get<0>(client::base
					   ::do_connect(client(this)));
		} catch(const x::exception &e) {
		}
	}

	return p;
}

clientObj::requestObj::requestObj(const x::ptr<x::obj> &mcguffinArg)
	: reqmcguffin(mcguffinArg)
{
}

clientObj::requestObj::~requestObj()
{
}

x::ref<x::obj> clientObj::requestObj::mcguffin()
{
	{
		std::lock_guard<std::mutex> lock(objmutex);

		x::ptr<x::obj> m=reqmcguffin.getptr();

		if (!m.null())
			return m;
	}

	return x::ref<x::obj>::create();
}

bool clientObj::requestObj::done()
{
	return reqmcguffin.getptr().null();
}

void clientObj::requestObj::wait()
{
	x::destroy_callback cb=x::destroy_callback::create();

	mcguffin()->ondestroy([cb]{cb->destroyed();});

	cb->wait();
}

void clientObj::shutdown()
{
	x::destroy_callback flag=x::destroy_callback::create();

	{
		implptr c=conn();

		if (c.null())
			return;

		c->shutdown();
		c->ondestroy([flag]{flag->destroyed();});
	}
	flag->wait();
}

bool clientObj::disconnected()
{
	return impl->getptr().null();
}

void clientObj::disconnect()
{
	x::ptr<implObj> ptr=impl->getptr();

	if (!ptr.null())
	{
		x::destroy_callback flag=
			x::destroy_callback::create();

		ptr->stop();
		ptr->ondestroy([flag]{flag->destroyed();});
		ptr=x::ptr<implObj>();
		flag->wait();
	}
}

x::fd clientObj::implObj::connect_socket(const std::string &filename)

{
	x::timerfd tfd=x::timerfd::create();

	tfd->set(0, clientObj::implObj::connect_timeout.getValue());

	x::fd sock=x::netaddr::create(SOCK_STREAM, "file:" + filename)
		->connect(x::fdtimeoutconfig::terminate_fd(tfd));

	sock->nonblock(true);

	while (!sock->send_credentials())
	{
		struct pollfd pfd;

		pfd.fd=sock->getFd();
		pfd.events=POLLOUT;

		if (::poll(&pfd, 1, -1) < 0)
		{
			if (errno != EINTR)
				throw SYSEXCEPTION("poll");
			continue;
		}

		if (pfd.revents & (POLLHUP|POLLERR))
			throw EXCEPTION(lost_connection_errmsg);
	}

	return sock;
}

void clientBase::defaultnodes(std::set<std::string> &paths)
{
	x::dir d=x::dir::create(clientObj::implObj::default_nodedir.getValue());

	for (auto &iter:*d)
		paths.insert(iter.fullpath());
}

client clientBase::connect()
{
	std::set<std::string> s;

	defaultnodes(s);

	if (s.empty())
		throw EXCEPTION("No object repositories found in " +
				clientObj::implObj::default_nodedir.getValue());

	if (s.size() > 1)
		throw EXCEPTION("There are more than one repository in " +
				clientObj::implObj::default_nodedir.getValue());

	return connect(*s.begin());
}

client clientBase::connect(const std::string &dir)
{
	auto c=connect_client(dir);

	connect_socket(c);

	return c;
}

client clientBase::admin(const std::string &dir)

{
	auto c=admin_client(dir);

	connect_socket(c);

	return c;
}

client clientBase::connect_client(const std::string &dir)
{
	return create_handle(STASHER_NAMESPACE::pubsockname(dir));
}

client clientBase::admin_client(const std::string &dir)

{
	return create_handle(STASHER_NAMESPACE::privsockname(dir));
}

client clientBase::create_handle(const std::string &sockname)
{
	return x::ptrrefBase::objfactory<client>::create(sockname);
}

void clientBase::connect_socket(const client &cl)
{
	x::destroy_callback cb=x::destroy_callback::create();

	connstatus status=({
			do_connect_t conn=do_connect(cl);

			std::get<1>(conn)->mcguffin()
				->ondestroy([cb]{cb->destroyed();});

			std::get<2>(conn);
		});

	cb->wait();

	if (status->result != req_processed_stat)
		throw EXCEPTION(cl->sockname + ": " +
				x::tostring(status->result));
}

// Kick off a connection thread

// The client object is presumed to have no active implementation thread that's
// running. Instantiate an implementation object, spawn it, and install it
// into the client object.
//
// Returns a tuple.
//
// first: the generic request object.
//
// second: the connection status object. When the client thread completes
// the connection request, according to the request object, see the connection
// results here.

clientBase::do_connect_t clientBase::do_connect(const client &cl)
{
	// A mcguffin for the connection thread. The implementation thread
	// releases the reference on this mcguffin when it completes the
	// connection attempt.

	x::ref<x::obj> mcguffin=x::ref<x::obj>::create();

	auto impl=clientObj::implref::create(cl->sockname);

	do_connect_t ret(impl, request::create(mcguffin), connstatus::create());

	// Mcguffin for the connection thread.

	x::ref<x::obj> thread_mcguffin(x::ref<x::obj>::create());

	x::start_threadmsgdispatcher(impl, x::ptr<x::obj>(mcguffin),
			connstatusptr(std::get<2>(ret)), thread_mcguffin);

	cl->impl->install(impl, thread_mcguffin);

	return ret;
}

void clientObj::implObj::run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
			     x::ptr<x::obj> &conn_mcguffin,
			     clientBase::connstatusptr &conn_status,
			     const x::ref<x::obj> &mcguffin)
{
	msgqueue_auto msgqueue(this);
        threadmsgdispatcher_mcguffin=nullptr;

	bool connected=false;

	try {
		x::fd socket=connect_socket(sockname);

		connected=true;

		conn_status->result=req_processed_stat;
		conn_status=clientBase::connstatusptr();

		stoppableThreadTrackerImpl tracker=
			stoppableThreadTrackerImpl::create();

		limits_mcguffin= &conn_mcguffin;

		reqs_t reqsmap;

		reqs=&reqsmap;

		std::queue<x::ptr<writtenObj<usergetuuids> > > getqueueref;

		getqueue=&getqueueref;
		getpending_cnt=0;

		subscriber_map_t subscriber_mapref;

		subscriber_map=&subscriber_mapref;

		server_status_subscribers_t server_status_subscribers_onstack;
		server_status_subscribers=&server_status_subscribers_onstack;
		server_status_subscribed=false;
		have_last_quorum_status_received=false;

		update_server_status_subscription();
		// A noop, for completeness' sake

		mainloop(msgqueue,
			 socket,
			 x::fd::base::inputiter(socket),
			 tracker->getTracker(),
			 x::ptr<x::obj>());

	} catch (const x::stopexception &e)
	{
	} catch (const x::exception &e)
	{
		if (connected)
		{
			LOG_ERROR(e);
			LOG_TRACE(e->backtrace);
		}
	}
}

void clientObj::implObj::accept_fds(size_t n)
{
}

void clientObj::implObj::received_fd(const std::vector<x::fdptr> &fdArg)

{
	void (implObj::*p)(const std::vector<x::fdptr> &)
=received_fd_handler;

	received_fd_handler=&implObj::received_fd_unknown;

	(this->*p)(fdArg);
}

void clientObj::implObj::received_fd_unknown(const std::vector<x::fdptr> &fdArg)

{
	throw EXCEPTION("Unexpectedly received file descriptors from the server");
}

void clientObj::implObj::dispatch_shutdown()
{
	writer->write(x::ref<writtenObj<adminstop> >::create(adminstop()));
}

clientObj::transactionObj::transactionObj()
{
}

clientObj::transactionObj::~transactionObj()
{
}

void clientObj::implObj::dispatch_put(const x::ref<puttransactionObj> &transaction,
				      const x::ref<putresultsObj::recvObj> &results,
				      const x::ptr<x::obj> &mcguffin)
{
	if (transaction->empty())
	{
		set_putresults(results, req_processed_stat);
		return;
	}

	if (transaction->objects.size() > helo.limits.maxobjects)
	{
		set_putresults(results, req_toomany_stat);
		return;
	}

	for (auto putobj: transaction->objects)
	{
		if (putobj.name.size() > objrepoObj::maxnamesize)
		{
			set_putresults(results, req_name2big_stat);
			return;
		}

		if (!putobj.contents.null() &&
		    !putobj.contents->contents_filedesc.null() &&
		    !S_ISREG(putobj.contents->contents_filedesc->stat()
			     ->st_mode))
		{
			set_putresults(results, req_2large_stat);
			return;
		}
	}

	if (transaction->totsize() > helo.limits.maxobjectsize)
	{
		set_putresults(results, req_2large_stat);
		return;
	}

	userput up(transaction, helo.limits);

	up.write(x::ptr<fdobjwriterthreadObj>(writer));

	reqs->insert(std::make_pair(up.uuid, reqinfo(mcguffin,
						     results)));
}

void clientObj::implObj
::deserialized(const fdobjwriterthreadObj::sendfd_ready
	       &msg)
{
	writer->sendfd_proceed(msg);
}

void clientObj::implObj::deserialized(const userputreply &msg)
{
	reqs_t::iterator p=reqs->find(msg.uuid);

	if (p == reqs->end())
	{
		LOG_ERROR("Unable to find completed transaction");
		return;
	}

	set_putresults(p->second.resp, msg.status, msg.newobjuuid);
	reqs->erase(p);
}

void clientObj::implObj::set_putresults(const putrequest &req,
					const req_stat_t status,
					const x::uuid &newuuid)
{
	auto res=putresults::create();

	res->status=status;
	res->newuuid=newuuid;
	req->installmsg(res);
}

void clientObj::implObj::deserialized(const userhelo &msg)
{
	helo=msg;
	LOG_DEBUG("Connection with " << msg.nodename << " established: "
		  << helo.limits.maxobjects << " maximum objects, "
		  << helo.limits.maxobjectsize
		  << " maximum aggregate size per transaction," << std::endl
		  << helo.limits.maxsubs << " maximum subscriptions");
	*limits_mcguffin=nullptr;

	for (auto &subscriber: *server_status_subscribers)
		subscriber.callback->serverinfo(helo);
}

// ---------------------------------------------------------------------------

void clientObj::debugSetLimits(const STASHER_NAMESPACE::userinit &fakelimits)

{
	// For debugging reasons we expect the connection to be here.
	conn()->deserialized(STASHER_NAMESPACE::userhelo(fakelimits));
}

void clientObj::debugWaitDisconnection()
{
	implptr p=impl->getptr();

	if (!p.null())
	{
		x::destroy_callback flag=x::destroy_callback::create();

		p->ondestroy([flag]{flag->destroyed();});
		p=x::ptr<implObj>();
		flag->wait();
	}
}

// ----------------------------------------------------------------------------


contentsObj::contentsObj() : succeeded(false),
			     errmsg(clientObj::implObj::DISCONNECTED_MSG)
{
}

contentsObj::~contentsObj()
{
}

clientObj::getreqObj::getreqObj(): openobjects(false), admin(false)
{
}

clientObj::getreqObj::~getreqObj()
{
}

getresults clientObj::get(const std::set<std::string> &objnames,
			  bool openobjects,
			  bool admin)

{
	client::base::getreq req(client::base::getreq::create());

	req->openobjects=openobjects;
	req->admin=admin;
	req->objects=objnames;
	return get(req);
}

void clientObj::implObj::dispatch_get(const x::ref<getreqObj> &req,
				      const x::ref<getresultsObj::recvObj> &results,
				      const x::ptr<x::obj> &mcguffin)
{
	if (req->objects.size() > helo.limits.maxobjects)
	{
		contents resp=contents::create();

		resp->errmsg="Number of objects requested exceeds server's limits";
		results->installmsg(resp);
		return;
	}

	for (auto b=req->objects.begin(), e=req->objects.end(); b != e; ++b)
	{
		if (b->size() > objrepoObj::maxnamesize)
		{
			contents resp=contents::create();

			resp->errmsg="Object name too big";
			results->installmsg(resp);
			return;
		}
	}

	x::ref<writtenObj<usergetuuids> >
		wmsg=x::ref<writtenObj<usergetuuids> >::create();

	wmsg->msg.reqinfo->openobjects=req->openobjects;
	wmsg->msg.reqinfo->admin=req->admin;

	wmsg->msg.reqinfo->objects.insert(req->objects.begin(),
					  req->objects.end());

	reqs->insert(std::make_pair(wmsg->msg.requuid,
				    reqinfo(mcguffin,
					    x::ref<implObj::uuidgetinfoObj>
					    ::create(results))));
	getqueue->push(wmsg);
	sendgetrequests();

#ifdef	DEBUG_TEST_GETQUEUE_CLIENTIMPL_RECEIVED
	DEBUG_TEST_GETQUEUE_CLIENTIMPL_RECEIVED();
#endif
}

void clientObj::implObj::sendgetrequests()
{
	while (!getqueue->empty() && getpending_cnt < helo.limits.maxgets)
	{
		writer->write(getqueue->front());
		getqueue->pop();
		++getpending_cnt;
	}
}

void clientObj::implObj::deserialized(const usergetuuidsreply::openobjects &msg)

{
	fdgetuuid=msg.requuid;
	received_fd_handler=&implObj::received_fd_get;
}

void clientObj::implObj::received_fd_get(const std::vector<x::fdptr> &fdArg)

{
	reqs_t::iterator p=reqs->find(fdgetuuid);

	if (p != reqs->end())
	{
		x::ref<implObj::uuidgetinfoObj>(p->second.resp)->filedescs=
			fdArg;
	}
}

void clientObj::implObj::deserialized(const usergetuuidsreply &msg)

{
	reqs_t::iterator p=reqs->find(msg.requuid);

	if (p != reqs->end())
	{
		x::ref<implObj::uuidgetinfoObj> reqinfo=p->second.resp;

		// If we received file descriptors, combine them with this
		// response

		if (reqinfo->filedescs.size() == msg.uuids->size())
		{
			std::vector<std::pair<std::string, retrobj> >
				::iterator p=msg.uuids->begin();

			for (std::vector<x::fdptr>::iterator
				     b=reqinfo->filedescs.begin(),
				     e=reqinfo->filedescs.end(); b != e; ++b)
			{
				p->second.fd=*b;
				++p;
			}

		}

		auto res=getresults::create();

		contents uuids=res->objects;

		uuids->insert(msg.uuids->begin(), msg.uuids->end());

		uuids->succeeded=msg.succeeded;
		uuids->errmsg=msg.errmsg;

		reqinfo->req->installmsg(res);
		reqs->erase(p);

		--getpending_cnt;
		sendgetrequests();
	}

}

userhelo clientObj::gethelo()
{
	x::ptr<implObj::getheloObj> resp=
		x::ptr<implObj::getheloObj>::create();

	x::destroy_callback cb=x::destroy_callback::create();

	{
		x::ptr<x::obj> mcguffin=x::ptr<x::obj>::create();

		auto c=conn();

		if (!c.null())
			c->gethelo(resp, mcguffin);

		mcguffin->ondestroy([cb]{cb->destroyed();});
	}

	cb->wait();

	return *resp;
}

void clientObj::implObj::dispatch_gethelo(const x::ref<getheloObj> &limits,
					  const x::ptr<x::obj> &mcguffin)
{
	static_cast<userhelo &>(*limits)=helo;
}

// ----------------------------------------------------------------------------

clientObj::subscriberObj::subscriberObj()
{
}

clientObj::subscriberObj::~subscriberObj()
{
}

void clientObj::implObj::dispatch_subscribe(const std::string &objname,
					    const client::base::subscriber &subscriber,
					    const x::ref<subscriberesultsObj::recvObj> &results,
					    const x::ptr<x::obj> &mcguffin)
{
	LOG_DEBUG("subscribe: " + objname);

	if (objname.size() > objrepoObj::maxnamesize)
	{
		x::ref<subscriberesultsObj>
			res=x::ref<subscriberesultsObj>::create();

		res->status=req_name2big_stat;
		results->installmsg(res);
		return;
	}

	if (subscriber_map->find(objname) != subscriber_map->end())
	{
		LOG_TRACE(objname + ": subscription already exists");

		// There's an existing subscription, just piggy-back on it
		register_subscription(objname, subscriber, results,
				      req_processed_stat);
		return;
	}

	// Check the pending queue of subscription requests sent to the server
	// and awaiting confirmation. If this object name is in the queue, just
	// add it.

	subq_t::iterator p=subq.find(objname);

	if (p != subq.end())
	{
		LOG_TRACE(objname +
			  ": subscription request already pending");

		p->second.push_back({objname, subscriber, results, mcguffin});
		return;
	}

	// Not subscribed, or subscribing to this object. Start a subscription
	// request.

	x::ref<writtenObj<STASHER_NAMESPACE::beginsubreq> > wmsg=
		x::ref<writtenObj<STASHER_NAMESPACE::beginsubreq> >::create();

	LOG_TRACE(objname + ": subscription request "
		  + x::tostring(wmsg->msg.requuid));

	wmsg->msg.objname=objname;

	writer->write(wmsg);

	subquuids.insert(std::make_pair(wmsg->msg.requuid, objname));
	subq[objname].push_back({objname, subscriber, results, mcguffin});
}

void clientObj::implObj::deserialized(const beginsubreply &msg)

{
	LOG_DEBUG(x::tostring(msg.requuid) + ": subscription acknowledged: "
		  + x::tostring(msg.msg->status));

	// Find the queue, and ack the waiting requests.

	subq_t::iterator p=({
			auto iter=subquuids.find(msg.requuid);

			if (iter == subquuids.end())
				throw EXCEPTION("Subscription ack with an "
						" unknown uuid "
						+ x::tostring(msg.requuid));

			std::string objname=iter->second;
			subquuids.erase(iter);

			subq.find(objname);
		});

	if (p == subq.end())
		throw EXCEPTION("Subscription ack for an unknown object");

	for (auto b=p->second.begin(),
		     e=p->second.end(); b != e; ++b)
		register_subscription(b->objname,
				      b->subscriber,
				      b->results, msg.msg->status);

	subq.erase(p);
}

void clientObj::implObj::register_subscription(const std::string &objname,
					       const client::base::subscriber &subscriber,
					       const x::ref<subscriberesultsObj::recvObj> &results,
					       req_stat_t status)
{
	LOG_DEBUG("Registered subscription for " << objname <<
		  x::tostring(status));

	x::ref<subscriberesultsObj>
		res=x::ref<subscriberesultsObj>::create();

	// Create a mcguffin, whose destructor will send an unsubscribe
	// message, here.

	auto mcguffin=x::ref<subscriptionMcguffinObj>::create();

	mcguffin->iterator=subscriber_map
		->insert(std::make_pair(objname,
					std::make_pair(subscriber,
						       res->cancel_mcguffin)));
	mcguffin->client=x::ptr<implObj>(this);

	res->status=status;
	res->mcguffin=mcguffin;
	results->installmsg(res);
}

void clientObj::implObj
::dispatch_unsubscribe(const subscriber_map_t::iterator &iterator)
{
	std::string objname=iterator->first;

	LOG_DEBUG("unsubscribe: " + objname);
	subscriber_map->erase(iterator);

	if (subscriber_map->find(objname) != subscriber_map->end())
	{
		LOG_TRACE("Other subscribers remain");
		return;
	}

	LOG_TRACE("No other subscribers for " + objname
		  + ", sending an subscribe request");

	x::ref<writtenObj<STASHER_NAMESPACE::endsubreq> > wmsg=
		x::ref<writtenObj<STASHER_NAMESPACE::endsubreq> >::create();

	wmsg->msg.objname=objname;

	writer->write(wmsg);
}

void clientObj::implObj::deserialized(const endsubreply &msg)

{
}

void clientObj::implObj::deserialized(const userchanged &msg)

{
	LOG_TRACE("Updated: " << msg.objname);

	for (auto i=subscriber_map->equal_range(msg.objname);
	     i.first != i.second; ++i.first)
	{
		LOG_TRACE("Notifying: " << msg.objname);
		i.first->second.first->updated(msg.objname, msg.suffix);
	}
}

userinit clientObj::getlimits()
{
	auto c=conn();

	if (!c.null())
		return c->getlimits();
	return userinit();
}

void clientObj::implObj
::dispatch_subscribeserverstatus(const serverstatuscallback &callback,
				 const subscribeserverstatusrequest &req,
				 const x::ref<x::obj> &mcguffin)
{
	LOG_DEBUG("subscribeserverstatus");
	auto res=x::ref<subscribeserverstatusresultsObj>::create();

	auto subscription_mcguffin=
		x::ref<serverstatusSubscriptionMcguffinObj>::create();

	subscription_mcguffin->subscriber=server_status_subscribers->
		emplace(server_status_subscribers->end(), callback,
			res->cancel_mcguffin,
			mcguffin, res);

	subscription_mcguffin->conn=x::ptr<implObj>(this);

	res->status=req_processed_stat;
	res->mcguffin=subscription_mcguffin;

	req->installmsg(res);

	if (limits_mcguffin->null())
		callback->serverinfo(helo);
	// Wait until the server responds

	if (!update_server_status_subscription())
	{
		LOG_TRACE("subscribeserverstatus: already subscribed, acking");
		subscription_mcguffin->subscriber->ack();
		callback->state(last_quorum_status_received);
	}
}

void clientObj::implObj
::dispatch_unsubscribeserverstatus(const server_status_subscribers_t::iterator &subscriber)
{
	server_status_subscribers->erase(subscriber);
	update_server_status_subscription();
}

bool clientObj::implObj::update_server_status_subscription()
{
	bool has_subscribers= !server_status_subscribers->empty();

	if (has_subscribers == server_status_subscribed)
		return false;

	LOG_TRACE("update_server_status_subscription: "
		  << has_subscribers);

	writer->write( x::ref<writtenObj<sendupdatesreq> >::create
		       (has_subscribers));
	server_status_subscribed=has_subscribers;
	return true;
}

void clientObj::implObj::deserialized(const STASHER_NAMESPACE::clusterstate &msg)
{
	LOG_DEBUG("received clusterstate");

	if (have_last_quorum_status_received &&
	    last_quorum_status_received == msg)
		return; // Noise

	have_last_quorum_status_received=true;
	last_quorum_status_received=msg;

	LOG_TRACE("Acking subscribers");
	for (auto &subscriber: *server_status_subscribers)
	{
		subscriber.ack();
		subscriber.callback->state(last_quorum_status_received);
	}
}

// ----------------------------------------------------------------------------

dirmsgreply::dirmsgreply()
	: msg(x::ptr<getdirresultsObj>::create())
{
}

dirmsgreply::dirmsgreply(const x::uuid &requuidArg)
 : requuid(requuidArg)
{
}

dirmsgreply::~dirmsgreply()
{
}

// ----------------------------------------------------------------------------

#include "client.auto.C"

STASHER_NAMESPACE_END

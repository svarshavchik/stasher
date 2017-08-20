/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "stasher/manager.H"
#include "stasher/currentbase.H"

#include <x/property_value.H>
#include <x/weakptr.H>
#include <x/destroy_callback_wait4.H>
#include <x/threads/timer.H>
#include <x/mpobj.H>

#include <set>

LOG_CLASS_INIT(STASHER_NAMESPACE::managerObj);

STASHER_NAMESPACE_START

// The mcguffin that we return to the application: when it goes out of scope
// and gets destroyed, we must make sure that the stasher::client handle in the
// managedObjectObj is also gone.
//
// There is a race condition that a timer callback is currently poking its
// nose in it. Therefore, we'll need to make sure that it's gone, using
// x::destroy_callback_wait4.

class LIBCXX_HIDDEN managerObj::retmcguffinObj : virtual public obj {

public:
	x::ref<x::obj> impl;

	retmcguffinObj(const x::ref<x::obj> &implArg) : impl(implArg)
	{
	}

	~retmcguffinObj()
	{
	}
};

class LIBCXX_HIDDEN managerObj::retmcguffinBaseObj {

 public:

	template<typename ptrType> class objfactory {

	public:
		// ondestroy() should not be invoked in the constructor.
		// Do this, instead.

		static ptrType create(const x::ref<x::obj> &implArg)
		{
			// Use the default create(), first.

			ptrType p=x::ptrref_base::objfactory<ptrType>
				::create(implArg);

			auto cb=x::destroy_callback_wait4::create(implArg);

			p->ondestroy([cb]{cb->destroyed();});
			return p;
		}
	};
};


class LIBCXX_HIDDEN managerObj::implObj : virtual public x::obj {

 public:

	x::property::value<x::hms> resubscribe_interval;

	class meta_t {
	public:
		// A timer that schedules retry requests
		x::timer timer;

		// The manager object's destructor was invoked

		bool destroying=false;

		meta_t() : timer(x::timer::create())
		{
			timer->setTimerName("stashermanager");
		}

		~meta_t()
		{
			timer->cancel();
		}
	};

	x::mpobj<meta_t> meta;

	implObj(const std::string &propname,
		const x::hms &propvalue)
		: resubscribe_interval(propname, std::move(propvalue))
	{
	}

	~implObj()
	{
	}

	class objectsubscriptioninfo {

	public:
		std::string objname;
		managedsubscriber subscriber;

		typedef subscriberequest request_t;

		objectsubscriptioninfo(const std::string &objnameArg,
				       const managedsubscriber &subscriberArg)
			: objname(objnameArg), subscriber(subscriberArg)
		{
		}

		std::string describe() const
		{
			return "subscription for \"" + objname + "\"";
		}

		auto request(const client &cl)
			-> decltype(cl->subscribe_request(objname, subscriber))
		{
			return cl->subscribe_request(objname, subscriber);
		}

		void connection_update(req_stat_t &status)
		{
			try {
				subscriber->connection_update(status);
			} catch (const x::exception &e)
			{
				LOG_ERROR(e);
				LOG_TRACE(e->backtrace);
			}
		}
	};

	class serverstatusupdateinfo {

	public:
		managedserverstatuscallback callback;

		typedef subscribeserverstatusrequest request_t;

		serverstatusupdateinfo(const managedserverstatuscallback
				       &callbackArg)
			: callback(callbackArg)
		{
		}

		std::string describe() const
		{
			return "server status subscription";
		}

		auto request(const client &cl)
			-> decltype(cl->subscribeserverstatus_request(callback))
		{
			return cl->subscribeserverstatus_request(callback);
		}

		void connection_update(req_stat_t &status)
		{
			try {
				callback->connection_update(status);
			} catch (const x::exception &e)
 			{
				LOG_ERROR(e);
				LOG_TRACE(e->backtrace);
			}
		}
	};

	template<typename info> class managedObjectObj;
	template<typename info> class objectsubscribeRequestObj;
	template<typename info> class objectsubscribeCancelledObj;

	template<typename info>
		void start(const x::ref<managedObjectObj<info> >
			   &subscriber);

	class managedObjectSubscriberObj;
};

managerObj::managerObj() : impl(x::ref<implObj>::create("objrepo::manager",
							x::hms(0, 1, 0)))
{
}

managerObj::managerObj(const std::string &property_name,
		       const x::hms &default_value)
	: impl(x::ref<implObj>::create(property_name, default_value))
{
}

managerObj::~managerObj()
{
	x::mpobj<implObj::meta_t>::lock lock(impl->meta);

	lock->destroying=true;
	lock->timer->cancel();
}

// A managed subscriber

// This is also the mcguffin that's given back to the user.

template<typename info>
class LIBCXX_HIDDEN managerObj::implObj::managedObjectObj
	: virtual public x::obj {
public:
	// The client connection
	client savedclient;

	// The manager that's managing this subscription
	x::weakptr<x::ptr<implObj> > manager;

	// The subscription implementation

	info impl;

	// This is the mcguffin for the actual client subscription. When this
	// object goes out of scope, it, in turn, releases the only reference
	// to the actual subscription.
	x::ptr<x::obj> current_mcguffin;

	// Status of the original subscription request

	req_stat_t request_stat;

	template<typename ...Args>
	managedObjectObj(const client &savedclientArg,
			 const x::ptr<implObj> &managerArg,
			 Args && ...args)
		: savedclient(savedclientArg),
		manager(managerArg),
		impl(std::forward<Args>(args)...)
	{
		LOG_TRACE("Constructed " << impl.describe());
	}

	~managedObjectObj()
	{
		LOG_TRACE("Destroyed " << impl.describe());
	}
};

// A destructor callback on the subscription request to a connected client.

// This is set as a destructor callback on the request mcguffin. When the
// destructor callback gets invoked, it notifies the user's managed subscription
// then sets up a destructor callback on the cancelled subscription mcguffin,
// so when the subscription gets cancelled, when the client connection went
// away, a resubscription gets rescheduled.

template<typename info>
class LIBCXX_HIDDEN managerObj::implObj::objectsubscribeRequestObj
	: virtual public x::obj {

public:
	// The subscription request
	typename info::request_t req;

	// The subscriber object.
	x::weakptr<x::ptr<managedObjectObj<info> > > subscriber;

	objectsubscribeRequestObj(const typename info::request_t &reqArg,
				  const x::ref<managedObjectObj<info> >
				  &subscriberArg)
		: req(reqArg), subscriber(subscriberArg)
	{
	}


	~objectsubscribeRequestObj() {
	}

	void destroyed();
};

// The destructor callback for the subscription cancellation mcguffin.

template<typename info>
class LIBCXX_HIDDEN managerObj::implObj::objectsubscribeCancelledObj
	: virtual public x::obj {

public:
	x::weakptr<x::ptr<managedObjectObj<info> > > savedsubscriber;

	std::string describe;

	objectsubscribeCancelledObj(const x::ptr<managedObjectObj<info> >
				    &subscriberArg)
		: savedsubscriber(subscriberArg),
		describe(subscriberArg->impl.describe())
	{
	}

	~objectsubscribeCancelledObj()
	{
	}

	// Subscription has been cancelled, schedule a resubscribe
	void destroyed();

	// This also serves as a timer job that gets invoked to resubscribe.
	void run() noexcept;

	// Recover weak pointers, and see if we can proceed

	void recover(void (objectsubscribeCancelledObj::*go)
		     (const x::ref<managedObjectObj<info> > &subscriber,
		      const x::ref<implObj> &manager));

	void go_reschedule(const x::ref<managedObjectObj<info> >
			   &subscriber,
			   const x::ref<implObj> &manager);

	void go_retry(const x::ref<managedObjectObj<info> > &subscriber,
		      const x::ref<implObj> &manager);
};

// User request to manage a subscription

x::ref<x::obj> managerObj::manage_subscription(const client &clientArg,
					       const std::string &objname,
					       const managedsubscriber &objsubscriber)
{
	LOG_DEBUG("New subscription to " << objname);

	auto subscriber=x::ref<implObj::managedObjectObj
			       <implObj::objectsubscriptioninfo> >
		::create(clientArg, impl, objname, objsubscriber);

	impl->start(subscriber);

	return retmcguffin::create(subscriber);
}

// User request to manage server status updates

x::ref<x::obj>
managerObj::manage_serverstatusupdates(const client &clientArg,
				       const managedserverstatuscallback &callbackArg)
{
	LOG_DEBUG("New server status update callback");

	auto callback=x::ref<implObj::managedObjectObj
			     <implObj::serverstatusupdateinfo> >
		::create(clientArg, impl, callbackArg);

	impl->start(callback);
	return retmcguffin::create(callback);
}

LOG_FUNC_SCOPE_DECL(STASHER_NAMESPACE::managerObj::implObj, impl_logger);

template<typename info>
void managerObj::implObj::start(const x::ref<managedObjectObj<info> >
				&subscriber)
{
	LOG_FUNC_SCOPE(impl_logger);

	LOG_DEBUG("Request: " << subscriber->impl.describe());

	auto req=subscriber->impl.request(subscriber->savedclient);

	auto mcguffin=req.second->mcguffin();

	auto cb=x::ref<objectsubscribeRequestObj<info> >
		::create(req.first, subscriber);
	mcguffin->ondestroy([cb]{cb->destroyed();});
}

template<typename info>
void managerObj::implObj::objectsubscribeRequestObj<info>::destroyed()
{
	LOG_FUNC_SCOPE(impl_logger);

	try {
		auto subscriber_ptr=subscriber.getptr();

		if (subscriber_ptr.null())
		{
			LOG_TRACE("Subscriber has been destroyed, ignoring");
			return;
		}

		auto &sub=*subscriber_ptr;

		auto msg=req->getmsg();
		LOG_TRACE("Processed: " << sub.impl.describe()
			  << ": "
			  << x::tostring(msg->status));

		switch (sub.request_stat=msg->status) {
		case req_processed_stat:
		case req_disconnected_stat:
			sub.current_mcguffin=msg->mcguffin;

			{
				auto cb=x::ref<objectsubscribeCancelledObj
					       <info> >
					::create(subscriber_ptr);

				msg->cancel_mcguffin
					->ondestroy([cb]{cb->destroyed();});
			}
			break;
		default:
			sub.current_mcguffin=nullptr;
			break;
		}

		sub.impl.connection_update(msg->status);
	} catch (const x::exception &e)
	{
		LOG_ERROR(e);
		LOG_TRACE(e->backtrace);
	}
}

template<typename info>
void managerObj::implObj::objectsubscribeCancelledObj<info>
::recover(void (objectsubscribeCancelledObj::*go)
	  (const x::ref<managedObjectObj<info> > &subscriber,
	   const x::ref<implObj> &manager))
{
	auto subscriber=savedsubscriber.getptr();

	if (subscriber.null())
	{
		LOG_TRACE("Cancelled: " << describe);
		return;
	}

	auto manager=subscriber->manager.getptr();

	if (manager.null())
	{
		LOG_TRACE("Cancelled: " << describe << ": manager destroyed");
		return;
	}

	(this->*go)(subscriber, manager);
}

template<typename info>
void managerObj::implObj::objectsubscribeCancelledObj<info>::destroyed()
{
	LOG_FUNC_SCOPE(impl_logger);

	try {
		recover(&managerObj::implObj::objectsubscribeCancelledObj<info>
			::go_reschedule);

	} catch (const x::exception &e)
	{
		LOG_ERROR(e);
		LOG_TRACE(e->backtrace);
	}
}

template<typename info>
void managerObj::implObj::objectsubscribeCancelledObj<info>
::go_reschedule(const x::ref<managedObjectObj<info> > &subscriber,
		const x::ref<implObj> &manager)
{
	{
		auto interval=manager->resubscribe_interval.getValue();

		x::mpobj<implObj::meta_t>::lock lock(manager->meta);

		if (lock->destroying)
			return;

		if (subscriber->request_stat != req_processed_stat)
		{
			auto me=x::ref<objectsubscribeCancelledObj<info>
				       >(this);

			auto task=x::timertask::base
				::make_timer_task([me] { me->run(); });

			LOG_TRACE("Scheduled in "
				  << x::tostring(interval) << ": " << describe);

			lock->timer->
				scheduleAfter(task,
					      std::chrono::seconds
					      (interval.seconds()));
			return;
		}
	}

	// The original subscription request was processed. It was a good
	// subscription. It's cancelled, so no reason to diddle around for the
	// timeout to expire. Go back up on that horse immediately, and if that
	// fails, then we'll cool off for the duration of the timeout.

	go_retry(subscriber, manager);
}

template<typename info>
void managerObj::implObj::objectsubscribeCancelledObj<info>::run() noexcept
{
	LOG_FUNC_SCOPE(impl_logger);

	try {
		recover(&managerObj::implObj::objectsubscribeCancelledObj<info>
			::go_retry);
	} catch (const x::exception &e)
	{
		LOG_ERROR(e);
		LOG_TRACE(e->backtrace);
	}
}

template<typename info>
void managerObj::implObj::objectsubscribeCancelledObj<info>
::go_retry(const x::ref<managedObjectObj<info> > &subscriber,
	   const x::ref<implObj> &manager)
{
	manager->start(subscriber);
}

// =========================================================================

// A managed object's subscriber. A managed object is implemented by subscribing
// to the object, and when notified via connection_update() or updated(),
// issuing a get() request, then passing its results to the
// provided callback.

class managerObj::implObj::managedObjectSubscriberObj
	: public managedsubscriberObj {

public:
	std::string objname;

	// The callbacks get invoked from the connection thread. The connection
	// thread has an indirect strong reference to this object, so having
	// a strong reference to the client handle would created a strong
	// reference, so it must be weak.

	x::weakptr<clientptr> cl;
	managedobject callback;

	managedObjectSubscriberObj(const std::string &objnameArg,
				   const client &clArg,
				   const managedobject &callbackArg)
		: objname(objnameArg), cl(clArg), callback(callbackArg)
	{
		LOG_TRACE("Constructed managed subscriber for " << objname);
	}

	~managedObjectSubscriberObj() {
		LOG_TRACE("Destroyed managed subscriber for " << objname);
	}

	// If connected, request the initial value of the object.

	void connection_update(req_stat_t status)
	{
		try {
			LOG_DEBUG("Received connection update: "
				  << objname << ": "
				  << x::tostring(status));
			callback->connection_update(status);
			if (status == req_processed_stat)
				update();
		} catch (const x::exception &e)
		{
			LOG_ERROR(e);
			LOG_TRACE(e->backtrace);
		}
	}

	// When the object has changed, request its new value

	void updated(const std::string &objnameArg,
		     const std::string &suffixArg)
	{
		LOG_DEBUG("Updated: " << objname);
		update();
	}

	// A mcguffin for the get() request

	// When it gets processed, pass through the object's retrieved
	// contents to the callback.

	class mcguffinObj : virtual public x::obj {

	public:
		x::ref<managedObjectSubscriberObj> man;
		client::base::getreq req;
		getrequest contentsreq;

		mcguffinObj(const x::ref<managedObjectSubscriberObj> &manArg,
			    const client::base::getreq &reqArg,
			    const getrequest &contentsreqArg)
			: man(manArg), req(reqArg), contentsreq(contentsreqArg)
		{
		}

		~mcguffinObj() {
		}

		void destroyed()
		{
			try {
				auto p=contentsreq->getmsg()->objects;

				if (!p->succeeded)
				{
					LOG_DEBUG(man->objname << ": "
						  << p->errmsg);
					man->callback->connection_update(req_failed_stat);
					return;
				}

				auto iter=p->find(man->objname);

				if (iter == p->end())
					man->callback->removed(man->objname);
				else
					man->callback
						->updated(man->objname,
							  iter->second.uuid,
							  iter->second.fd);
			} catch (const x::exception &e)
			{
				LOG_ERROR(man->objname << ": " << e);
				LOG_TRACE(e->backtrace);
			}
		}
	};

	// Request the object's current value.
	void update()
	{
		clientptr clptr=cl.getptr();

		if (clptr.null())
		{
			LOG_DEBUG("Managed object update cancelled: "
				  << objname << ": client handle destroyed");
			return;
		}

		auto req=client::base::getreq::create();

		req->objects.insert(objname);
		req->openobjects=true;

		auto getcontents = clptr->get_request(req);

		clptr=clientptr();

		auto mcguffin=x::ref<mcguffinObj>
			::create(x::ref<managedObjectSubscriberObj>(this),
				 req, getcontents.first);

		getcontents.second->mcguffin()
			->ondestroy([mcguffin]{mcguffin->destroyed();});
	}
};

x::ref<x::obj> managerObj::manage_object(const client &clientArg,
					 const std::string &objname,
					 const managedobject &mon)
{
	auto subscriber=x::ref<implObj::managedObjectSubscriberObj>
		::create(objname, clientArg, mon);
	if (objname.size() == 0 || *--objname.end() == '/')
	{
		mon->connection_update(req_badname_stat);
		return subscriber;
	}

	return manage_subscription(clientArg, objname, subscriber);
}

void managerObj::handle_exception(const x::exception &e)
{
	LOG_FATAL(e);
	LOG_TRACE(e->backtrace);
}

STASHER_NAMESPACE_END

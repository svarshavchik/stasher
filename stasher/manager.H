/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_manager_H
#define stasher_manager_H

/*! \file */

#include <x/ref.H>
#include <x/obj.H>
#include <x/mpobj.H>
#include <x/logger.H>
#include <x/property_valuefwd.H>
#include <x/hms.H>
#include <stasher/namespace.H>
#include <stasher/client.H>
#include <stasher/managedsubscriber.H>
#include <stasher/managedserverstatuscallback.H>
#include <stasher/managedhierarchymonitor.H>
#include <stasher/managedobject.H>

#include <string_view>

STASHER_NAMESPACE_START

class managerObj;

//! Create a managedobject() that takes care of deserializing the object.

//! This function returns an argument that's suitable to be passed as the
//! \ref managedobject "managedobject" parameter to
//! \ref managerObj::manage_object() "manager->manage_object". The first
//! template parameter
//! must be an x::ptr to an object with a constructor that takes an x::uuid
//! and a file descriptor.
//!
//! create_managed_object()
//! constructs a managedobject subclass that implements the following:
//!
//! - The removed() callback instantiates the null x::ptr.
//!
//! - The updated() callback instantiates the the object, passing
//! the uuid, and the file descriptor to the object's constructor.
//!
//! The update functor gets invoked with two parameters:
//!
//! - The instantiated ptr (a null ptr, in case of removed()).
//!
//! - A boolean true, if this is the first call after a
//! connection_update(), indicating the initial value of the object,
//! or the value after a connection was established or reestablished.
//!
//! - A second functor receives the forwarded connection_update() callback.

template<typename ptr_type, typename updateFunctor,
	 typename connectionFunctor>
	 managedobject create_managed_object(updateFunctor && update,
					     connectionFunctor && connect);

//! When you don't care about the second functor.

//! \overload
//!
template<typename ptr_type, typename updateFunctor>
inline managedobject create_managed_object(updateFunctor && update)
{
	return create_managed_object(std::move(update),
				     [] (STASHER_NAMESPACE::req_stat_t) {}
				     );
}

//! Client manager.

//! \see manager

class managerObj : virtual public x::obj {

	//! The logging object
	LOG_CLASS_SCOPE;

	class implObj;

	//! Internal implementation object.
	x::ref<implObj> impl;

public:

	//! Constructor
	managerObj();

	//! Constructor, non-default retry property.

	managerObj(const std::string &property_name,
		   const std::string_view &default_value)
		: managerObj(property_name, x::hms{default_value})
	{
	}

	//! Constructor, non-default retry property.

	managerObj(const std::string &property_name,
		   const x::hms &default_value=x::hms(0, 1, 0));

	//! Destructor
	~managerObj();

	//! Create a managed subscription
	x::ref<x::obj> manage_subscription(const client &clientArg,
					   const std::string &objname,
					   const managedsubscriber &objsubscriber);

	//! Create a managed server status object callback
	x::ref<x::obj> manage_serverstatusupdates(const client &clientArg,
						  const managedserverstatuscallback &callbackArg);

	//! Create a managed hierarchy monitor object

	x::ref<x::obj> manage_hierarchymonitor(const client &clientArg,
					       const std::string &hiername,
					       const managedhierarchymonitor
					       &mon);

	//! Create a managed (monitored) object
	x::ref<x::obj> manage_object(const client &clientArg,
				     const std::string &objname,
				     const managedobject &mon);

private:

	// The class constructed by create_managed_object()

	template<typename ptr_type, typename updateFunctor,
		 typename connectionFunctor>
	class simpleManagedObjectObj;

	class retmcguffinObj;
	class retmcguffinBaseObj;

	//! The mcguffin that's actually returned by all the manage_() methods.

	typedef x::ref<retmcguffinObj, retmcguffinBaseObj> retmcguffin;

public:
	//! Handle exceptions from callbacks.

	//! Typically generated in create_managed_object()'s functors, which
	//! get invoked from client connection threads.
	//!
	//! Logs the exception, then eats it.

	static void handle_exception(const x::exception &e);

	template<typename ptr_type, typename updateFunctor,
		 typename connectionFunctor>
	friend managedobject
	create_managed_object(updateFunctor && update,
			      connectionFunctor && connect);
};

//! A wrapper for managedobjectObj constructed by create_managed_object()

//! This specialization enforces x::ptr as the first template parameter
//! to create_managed_object().

template<typename objClass, typename baseClass, typename updateFunctor,
	 typename connectionFunctor>
class managerObj::simpleManagedObjectObj<x::ptr<objClass, baseClass>,
					 updateFunctor,
					 connectionFunctor> :
	public managedobjectObj {

	//! Saved update functor

	typename std::enable_if<
		!std::is_lvalue_reference<updateFunctor>::value &&
	!std::is_rvalue_reference<updateFunctor>::value,
		updateFunctor>::type update;

	//! Saved connection functor
	typename std::enable_if<
		!std::is_lvalue_reference<connectionFunctor>::value &&
	!std::is_rvalue_reference<connectionFunctor>::value,
		connectionFunctor>::type connect;

	//! Flag: connection_update() was invoked
	bool connected;

public:
	//! Constructor saved the functors
	simpleManagedObjectObj(updateFunctor && updateArg,
			       connectionFunctor &&connectArg)
		: update(std::move(updateArg)),
		  connect(std::move(connectArg)), connected(true)
	{
	}

	//! Destructor
	~simpleManagedObjectObj()
	{
	}

	//! Notification from the manager about a change in connection status

	void connection_update(req_stat_t status)
	{
		try {
			// When a connection to the object repository server
			// gets established, the manager takes care of
			// requesting the new value, and will invoke updated()
			// or removed() shortly. Let them know that this is the
			// initial load from the object repository.
			connected=true;
			connect(status);
		} catch (const x::exception &e)
		{
			handle_exception(e);
		}
	}

	//! Manager giving us the updated object's contents.

	void updated(const std::string &objname,
		     const x::uuid &uuid,
		     const x::fd &contents)
	{
		bool flag=connected;

		connected=false;

		try {
			x::ptr<objClass, baseClass> ptr;

			// Try to construct its contents.
			try {
				ptr=x::ptr<objClass, baseClass>
					::create(uuid, contents);
			} catch (const x::exception &e)
			{
				handle_exception(e);
				// If an exception gets thrown, construct
				// a default object.

				ptr=x::ptr<objClass, baseClass>::create(uuid);
			}
			update(ptr, flag);
		} catch (const x::exception &e)
		{
			handle_exception(e);
		}
	}

	//! Notification from the manager that the object is not in the repository any more.

	void removed(const std::string &objname)
	{
		bool flag=connected;

		connected=false;

		try {
			update(x::ptr<objClass, baseClass>(), flag);
		} catch (const x::exception &e)
		{
			handle_exception(e);
		}
	}
};

template<typename ptr_type, typename updateFunctor,
	 typename connectionFunctor>
managedobject create_managed_object(updateFunctor && update,
				    connectionFunctor && connect)
{
	return x::ref< managerObj::simpleManagedObjectObj<ptr_type,
							  updateFunctor,
							  connectionFunctor> >
		::create(std::move(update),
			 std::move(connect));
}

//! High level client connection manager

//! \code
//! INSERT_STASHER_NAMESPACE::manager manager=INSERT_STASHER_MANAGER::create();
//! \endcode
//!
//! The client manager takes care of restoring a sane application state when
//! the connection to the object repository server goes down. The
//! \ref client "client connection" already automatically attempts to
//! reconnect to the server, with every request. However, when there's an
//! active object subscription, for example, it's the application's
//! responsibility to resubscribe. Use this object to take care of that, so
//! that the application, for example, only needs to set up a managed
//! subscription, and the manager will take care of automatically trying to
//! resubscribe, in face of failure.
//!
//! The default constructor sets the retry interval from an application
//! property named "objrepo::manager", which gets set to one minute, by default.
//!
//! \code
//! INSERT_STASHER_NAMESPACE::manager manager=INSERT_STASHER_MANAGER::create(L"reconnect", "30 seconds");
//! \endcode
//!
//! This example constructs a manager that uses a property named "reconnect",
//! whose default value is 30 seconds. The second parameter is an \c x::hms
//! that can be initialized from a literal string.
//!
//! \note
//! All the callbacks described below get invoked by the thread that handles
//! the connection with the object repository. The callbacks should not block,
//! or take an excessive time to return. Until the callback returns, no further
//! requests can be processed.
//!
//! \note
//! Additionally, the callback should not reference the client connection handle
//! except under very controlled conditions, where it is not possible for
//! the handle that's referenced by the callback being the last reference to
//! the connection handle. If a callback obtains a client connection handle
//! reference, and all other reference to the client connection handle go out
//! of scope in the meantime, a deadlock occurs when the callback's reference,
//! the last reference to the client handle, goes out of scope. A weak pointer
//! is not good enough, since a strong reference needs to get recovered from it,
//! for a short time, in order to make any use of it, and if the other
//! reference to the handle happen to go out of scope during that small window,
//! you're boned.
//!
//! Preferrably, the callbacks would typically be short functors that send some
//! kind of a notification message to a thread. An alternative solution would
//! be to store the client handle in the subclass, but have the callback object
//! owned by a x::DestroyCallbackFlag::base::guard that will go out of scope
//! just before the client connection handle reference, making sure that the
//! callback object, and its client handle reference, goes out of scope and
//! gets destroyed first. Then, it's your responsibility to make sure that
//! this doesn't deadlock, for other reasons.
//!
//! \par Managed subscriptions
//!
//! Inherit from managedsubscriberObj, rather than
//! \ref clientObj::subscriberObj "client::base::subscriberObj".
//! In addition to the updated() notification,
//! \ref managedsubscriberObj "managedsubscriberObj" adds the
//! connection_update() callback.
//!
//! Invoking "manager->manage_subscription()" results in the connection_update()
//! callback once the subscription request gets processed {MANAGEDSUBINIT],
//! followed by updated() callbacks, as usual [MANAGEDSUBUPD}. If the
//! subscription gets cancelled by
//! a dropped connection to the server, connection_update() callbacks will
//! follow, at regular intervals.
//!
//! \note
//! If connection_update() gets invoked with any status other than
//! \c req_processed_stat or \c req_disconnected_stat,
//! this indicates some other problem with the subscription, and the manager
//! will no longer manage the subscription.
//!
//! manager->manage_subscription() returns a mcguffin that represents a managed
//! subscription. The subscription maintains its own, internal, strong
//! reference on the underlying managedsubcriberObj. When the mcguffin goes
//! out of scope and gets destroyed, the managed subscription gets cancelled,
//! releasing its reference on the managedsubscriberObj.
//!
//! \note
//! The reference on the managedsubscriberObj may not get released by the
//! mcguffin's destructor, if a subscription request is pending to the client.
//! It should get released promptly, when the request gets processed.
//!
//! All subscriptions get automatically cancelled when the manager object goes
//! out of scope and gets destroyed.
//!
//! \par Monitoring a managed object's contents.
//!
//! Inherit from managedobjectObj, and implement its updated() and removed()
//! methods, as well as the connect_update() method that gets inherited from
//! its superclass.
//!
//! This is similar to a managed object subscription, except that each time
//! the object contents' change, the updated contents get retrieved and
//! passed to the updated() callback.
//!
//! Invoking "manager->manage_object()" results in the connection_update()
//! callback once the managed object subscription gets processed, followed
//! by an updated() or a removed() callback, indicating whether the object
//! currently exists, or not in the object repository.
//!
//! Thereafter, any time the object changes, the updated() or removed() callback
//! gets invoked, reporting the object's updated contents.
//!
//! If the connection to the server breaks, the connection_update()
//! callback getting invoked, with similar semantics as for a managed object
//! subscription.
//!
//! managed->manage_object() returns a mcguffin with a similar function as the
//! mcguffin that's returned by manager->manage_subscription().
//!
//! You can use create_managed_object() to create a light wrapper that
//! implements a managed object which constructs a reference-counted object,
//! passing the file descriptor and the uuid given to updated() to the
//! constructor and passing it to a functor, or passing a null x::ptr() to a
//! functor, in removed()'s case. An optional second functor gets invoked by
//! the wrapper's connection_update().
//!
//! \par Managed server status callback updates
//!
//! Inherit from managedserverstatuscallbackObj, rather than
//! serverstatuscallbackObj.
//! In addition to the quorum() notification,
//! \ref managedserverstatuscallbackObj "managedserverstatuscallbackObj"
//! adds the connection_update() callback.
//!
//! This is works like managed object subscriptions. Invoking
//! "managed->manage_serverstatusupdates()" registers a server status callback,
//! that gets reestablished automatically upon reconnecting to the client.
//! In addition to the quorum() callback getting periodically invoked
//! [MANAGEDSERVERSTATUSQUORUM], the connection_update() callback gets
//! invoked with \c req_processed_stat when the server status callback gets
//! succesfully registered, and \c req_disconnected_stat, when the connection
//! drops [MANAGEDSERVERSTATUSCONN].
//!
//! \note
//! Each managed subscriber counts towards the limit on the maximum number
//! of concurrent subscriptions a client may open, with the server.
//!
//! \par Managed hierarchy monitor
//!
//! \code
//! class myHierarchyMonitorObj : public managedhierarchymonitorObj {
//!
//! // ...
//!
//!     void connection_update(req_stat_t status);
//!
//!     void begin();
//!
//!	void enumerated();
//!
//!     void updated(const std::string &objname, const x::uuid &objuuid);
//!
//!     void removed(const std::string &objname);
//! };
//!
//! auto mon=x::ref<myHierarchyMonitorObj>::create();
//!
//! x::ref<x::obj> mcguffin=manage_hierarchymonitor(client, "app", mon);
//!
//! \endcode
//!
//! A managed hierarchy monitor expands the aspect of a managed
//! subscription into a protocol that delivers the initial contents of
//! a hierarchy, when a subscription gets established, and ongoing changes
//! to the hierarchy.
//!
//! The first parameter to manage_hierarchymonitor() is the client connection
//! to use. The second parameter gives the hierarchy's name, and the third
//! parameter is a reference to a subclass of managedhierarchymonitorObj that
//! implements the callback shown above.
//!
//! manage_hierarchymonitor() returns a mcguffin representing the managed
//! hierarchy monitoring. When the last reference to the mcguffin goes out of
//! scope and gets destroyed, the hierarchy monitoring stops.
//!
//! The manager maintains a strong reference on the callback object for the
//! duration of the subscription. When the subscription mcguffin goes out of
//! scope and gets destroyed, the reference gets released. If one of its
//! callback is in progress, the reference gets released when the callback
//! returns.
//!
//! All subscriptions get automatically cancelled when the manager object goes
//! out of scope and gets destroyed.
//!
//! connection_update() gets invoked under the same circumstances that the
//! underlying object subscription does. Also, a connection_update() with
//! a req_processed_stat status is followed by begin(), indicating that
//! the managed hierarchy monitoring set up is
//! complete (which usually happens soon after manage_hierarchymonitor()
//! returns). This is followed by updated() callbacks, enumerating the
//! existing contents of the hierarchy, at the time the monitoring started.
//! Afterwards, updated() and removed() get invoked to report any changes
//! to the contents of the hierarchy.
//!
//! updated() reports the full name of the object, in the monitored hierarchy,
//! and its current uuid. removed() reports the name of the object that was
//! removed from the monitored hierarchy.
//!
//! begin() gets followed by zero or more updated(), then enumerated(), to
//! define the initial contents of the monitored hierarchy.
//! [MANAGEDHIERMONENUM]
//! After enumerated(),
//! updated() and removed() get called to report changes to the the monitored
//! hierarchy. [MANAGEDHIERMONUPD]
//!
//! Like with a object subscription, connection_update(req_disconnected_stat)
//! gets invoked in the event the connection to the server
//! gets broken. When the connection gets reestablished, begin() gets invoked
//! once more, followed by the current contents of the monitored hierarchy,
//! reported via updated(). [MANAGEDHIERMONRECONNECT]
//!
//! The caller may treat either begin() as logically
//! equivalent to removing all existing contents of monitored hierarchy.
//!
//! updated() reports a new object's uuid, or the new uuid of a modified
//! object. It is possible for updated() to report an unchanged object uuid,
//! a second time. This might happen when the managed hierarchy gets set up
//! simultaneously with the object getting updated. For the same reason,
//! removed() may be called more than once to report the same object begin
//! removed.
//!
//! Finally, it's possible for removed() to be called after begin(), and
//! before enumerated(), to report changes to the hierarchy's contents that
//! were concurrent with its initial contents' enumeration. Generally,
//! the objects reported by updated() and removed() should be processed
//! accordingly: the given object added to the client's understanding of what
//! the hierarchy contains, if client does not know about it yet; or the object
//! removed from the client's view of the monitored hierarchy, if it is there.
//!
//! It's possible for connection_update(req_disconnected_stat)
//! to be called after begin() and before
//! enumerated(), if the connection broke before the hierarchy's initial
//! contents were enumerated.
//!
//! A connection_update() with other statuses, besides req_processed_stat and
//! req_disconnected_stat, indicates a monitoring problem. req_badname_stat,
//! for example, reports that the hierarchy name is not valid.
//!
//! \note
//! updated() and removed() report only on the immediate contents of the
//! monitored hierarchy. The contents of any subhierarchies do not get
//! reported (but may have their own managed subscriptions).
//!
//! \note
//! Each managed hierarchy counts towards the limit on the maximum number
//! of concurrent subscriptions a client may open, with the server.

typedef x::ref<managerObj> manager;

//! A nullable reference pointer to a \ref manager "connection manager".
typedef x::ptr<managerObj> managerptr;

STASHER_NAMESPACE_END

#endif

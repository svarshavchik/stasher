/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_clientfwd_H
#define stasher_clientfwd_H

/*! \file */

#include <x/ref.H>
#include <x/ptr.H>

#include <stasher/namespace.H>

STASHER_NAMESPACE_START

class LIBCXX_PUBLIC clientObj;
class LIBCXX_PUBLIC clientBase;

//! A connection to an object repository

//! \code
//! client cluser=client::base::connect(directory);
//!
//! ...
//!
//! client cladmin=client::base::admin(directory);
//!
//! ...
//!
//! client cluser=client::base::connect_client(directory);
//!
//! ...
//!
//! client cladmin=client::base::admin_client(directory);
//!
//! ...
//!
//! std::set<std::string> nodes;
//!
//! client::base::defaultnodes(nodes);
//!
//! client cluser=client::base::connect();
//!
//! \endcode
//!
//! \c directory is a file directory where an object repository daemon is
//! running. An exception gets thrown if a daemon is not running and accepting
//! connections.
//!
//! connect() creates a new connection and returns a handle to the connection
//! object. admin() creates a new connection with administrative privileges.
//! This can be done only by root or by the same userid that owns the directory.
//!
//! connect() and admin() throws an exception if the connection fails.
//! If, subsequent to the connection getting established, the server shuts
//! down and the connection breaks, any pending requests get failed with an
//! appropriate error code, and the next request automatically attempts to
//! reconnect to the server. No exception gets thrown if the reconnection
//! attempt fails, and the next request makes another connection attempt.
//!
//! In the mean time, all requests fail, with an appropriate error code,
//! until another connection gets established again.
//!
//! A \ref manager "connection manager", employs its own reconnection
//! strategy.
//!
//! connect_client() and admin_client() allocate a connection handle without
//! making a connection attempt. The first request, of any kind, initiates
//! a connection attempt. The request fails if the connection attempt fails.
//! No exception gets thrown, the request fails with an appropriate error code.
//!
//! defaultnodes() enumerates the repository directories that are installed in
//! the default location that's configured into the object repository library.
//! An object repository node directory can get installed anywhere.
//! If there's only one repository node installed on the server, in the
//! default installation directory (usually the case), defaultnodes() places
//! the directory into the string set. The directory can then get passed
//! directly to connect(), admin(), connect_client(), or admin_client().
//! If there are no nodes installed, or there's more than one, defaultnodes()
//! updates the string set accordingly, and leaves it up to you to figure out
//! where to go from there.
//!
//! Most applications will want to use connect() that takes no arguments.
//! That version calls defaultnodes(), and expects it to return a single
//! available object repository node; then it proceeds and makes the connection,
//! as if that directory was specified for the connect(). An exception gets
//! thrown if no node directories are found, or if there was more than one.
//!
//! \par Disconnecting from the repository
//!
//! \code
//! cluser->disconnect();
//! \endcode
//!
//! The connection to the repository is managed by a separate thread.
//! Invoking disconnect() sends a message to the thread to shut down.
//! disconnect() returns after the thread has stopped completely
//! [CLIENTDISCONNECT]. This connection is no longer usable.
//!
//! In most situations this is not necessary.
//! \c client is a reference to a reference-counted object.
//! A disconnect automatically occurs when the last reference to the client
//! object goes out of scope, and it gets destroyed.
//!
//! \note
//! Making another request, after disconnect(), results in an automatic
//! reconnection attempt, as if the connection to the server got disconnected
//! spontaneously.
//!
//! \par Shutting down the repository
//!
//! \code
//! cladmin->shutdown()
//! \endcode
//!
//! This repository server is stopped and shut down. All connections to the
//! the server, including this one, are disconnected. This method returns when
//! the connection to the server closes.
//!
//! This is an absolute stop. If this server was the cluster's master, the
//! remaining servers in the cluster elect a new master, and resynchronize.
//!
//! This call requires a connection with administrative privileges.
//!
//! \par Retrieving user limits
//!
//! \code
//! userinit limits=cluser->getlimits();
//! \endcode
//!
//! \ref clientObj::getlimits "getlimits()" returns an object that gives the
//! maximum resources the client can use. See \ref userinit "userinit" for
//! a list of all the fields. The client
//!
//! \par Putting objects into the repository
//!
//! \code
//!
//! std::string object_name;
//!
//! std::string object_value;
//!
//! x::fd object_valuefd;
//!
//! x::uuid existing_object_uuid;
//!
//! client::base::transaction tran=client::base::transaction::create();
//!
//!     tran->newobj(object_name, object_value);
//!
//!     tran->newobj(object_name, object_valuefd);
//!
//!     tran->updobj(object_name, existing_object_uuid, object_value);
//!
//!     tran->updobj(object_name, existing_object_uuid, object_valuefd);
//!
//!     tran->delobj(object_name, existing_object_uuid);
//!
//! \endcode
//!
//! A transaction object gets created for the purpose of putting or modifying
//! object in the repository. All objects referenced in the transaction object
//! get updated atomically.
//!
//! newobj() lists a new object that's to be added to the repository. updobj()
//! specifies that the existing object's value must be replaced, and the
//! object's uuid. The new object value is specified as either a literal string
//! value, or a file descriptor. The entire contents of the file descriptor
//! set the new object's value. delobj() names an object to be removed from
//! the repository.
//!
//! An existing object's uuid must be specified for modifying or deleting an
//! existing object. If the existing object's uuid does not match, the
//! transaction gets rejected. An attempt to add a new object to the repository
//! when the object already exists gets also rejected. If any object in the
//! transaction gets rejected, the entire transaction gets rejected, and no
//! changes specified in the transaction takes place.
//!
//! Modifying objects requires write privileges in the selected namespace
//! [WRITEPRIVILEGES].
//!
//! \code
//! putresults res=cl->put(tran);
//!
//! if (res->status != req_processed_stat)
//!     std::cerr << x::tostring(res->status) << std::endl;
//!
//! x::uuid newuuid=res->newuuid;
//! \endcode
//!
//! put() sends the transaction to the object repository server, waits for the
//! transaction to get processed, then returns an object that gives the status
//! of the transaction. If the transaction succeeded, \c newuuid gives the new
//! uuid of all objects updated by the transaction.
//!
//! \code
//! std::pair<putresults, x::ptr<x::obj> res=cl->put_request(tran);
//!
//!     x::destroyCallbackFlag cb=x::destroyCallbackFlag::create();
//!
//!     res.second->addOnDestroy(cb);
//!     res.second=nullptr;
//!     cb->wait();
//! \endcode
//!
//! put_request() is the asynchronous version of put(). put_request() does
//! not wait for the transaction to be processed, it gets sent to the server
//! and put_request() returns immediately, with an allocated results object,
//! and a mcguffin with the held reference. The held reference gets released
//! when the server processes the transaction, at which point the result object
//! will have the transaction's status.
//!
//! The results object may not be accessed while the reference on the mcguffin
//! remains held. [CLIENTPUT]
//!
//! \par The asynchronous request object
//!
//! Most methods have two versions, a synchronous and an asynchronous version.
//! A synchronous version processes the request, and returned the results.
//! The request often involves sending a message to the object repository
//! server. The synchronous version waits for the response, before returning.
//!
//! An asynchronous version sends a request and returns immediately. The
//! asynchronous version typically returns a
//! \ref clientBase::request "client::request" object. This request
//! object tracks the outstanding, pending request to the server.
//!
//! \code
//! client::base::request submitted_req;
//!
//! // ...
//!
//! x::ptr<x::obj> mcguffin=submitted_req->getMcguffin();
//!
//! // ...
//!
//! bool flag=submitted_req->done();
//!
//! // ...
//!
//! submitted_req->wait();
//! \endcode
//!
//! The done() method
//! returns an indication whether the request has been processed.
//! There are two ways to wait for the request to complete: invoke wait(),
//! which waits until the request completes, or getMcguffin() that returns
//! a mcguffin. getMcguffin() returns a null reference if the request
//! has already been processed. Otherwise a reference on the returned mcguffin
//! is held until the request has been processed.
//!
//! After the request has been received, the results of the original request
//! are accessible, as documented by each individual asynchronous method. In
//! most cases, the regular synchronous method invokes the asynchronous
//! version, calls wait(), then returns the results.
//!
//! \par Retrieving objects from the repository
//!
//! \code
//! client::base::contentsreq req=client::base::contentsreq::create();
//!
//! req->objects.insert("obj1");
//!
//! req->openobjects=true;
//!
//! contents res=cluser->get(req)->objects;
//!
//! if (!res->succeeded)
//!    throw res->errmsg;
//!
//! contents::base::map_t::iterator p=res->find("obj1");
//!
//! if (p != res->end())
//!    found(p->second.uuid, p->second.fd);
//! \endcode
//!
//! get() retrieves one or more objects from the server. The request
//! contains a \c std::set of object names to retrieve, and an \c bool
//! openobjects flag, \c false to retrieve only each object's uuid, and
//! \c true to retrieve the object's uuid and an open file descriptor for
//! the object's contents.
//!
//! The request object also contains a flag called "admin". Normally, the
//! objects are returned only if the object repository node is in quorum with
//! the cluster. If not, the get() request waits until quorum gets
//! established. If "admin" is set, the request is processed even if no quorum
//! is present. This flag is ignored unless the connection with the server has
//! administrative privileges [FORCEGETADMIN].
//!
//! get() returns a \ref getresults "getresults", whose only member of
//! importants is \c objects, a \ref contents "contents" reference that refers
//! to a
//! subclass of a \c std::map with two additional fields:
//! "succeeded", whether request succeeded; and a \c std::string errmsg that
//! holds an error message if the request failed.
//!
//! The map is keyed by the object name. An object that does not exist
//! is not considered an error. The returned map does not have keys for the
//! missing objects. The map's value for an existing object contains two
//! fields: the object's uuid, and a file descriptor for the object's contents.
//! The file descriptor is a null reference if the openobjects flag in the
//! request was not set.
//!
//! \code
//! std::set<std::string> objects;
//!
//! contents res=cluser->get(objects, true)->objects;
//! \endcode
//!
//! This is a convenience function that constructs the request object, sends
//! the request, and returns the contents object.
//!
//! \code
//! contents res=cluser->get("fruits/apple", "fruits/banana", true)->objects;
//! \endcode
//!
//! This is a convenience variadic template function that takes literal
//! object names passed as a variadic parameter list, with an optional
//! \c bool that sets the \c openobjects flag.
//!
//! \code
//! client::base::contentsreq req=client::base::contentsreq::create();
//!
//! req->objects.insert("obj1");
//!
//! req->openobjects=true;
//!
//! client::base::request submitted_req=cluser->get_request(req);
//!
//! submitted_req->wait();
//!
//! contents res=req->get()->objects;
//!
//! \endcode
//!
//! get_request() is the asynchronous version of get().
//! get_request() returns a
//! \ref clientBase::request "client::base::request".
//! After the request has been received, and only after that, invoking
//! get() on the original request object returns the results of the
//! request.
//!
//! \par Retrieving server status as a text report
//!
//! \code
//! getserverstatusresults status=cladmin->getserverstatus();
//!
//! std::cout << status->report;
//!
//! // ...
//!
//! std::pair<getserverstatusrequest, client::base::request> req=cladmin->getserverstatus_request();
//!
//! \endcode
//!
//! getserverstatus() returns the server's status. \c getserverstatusresults
//! is a reference to an object with a single \c std::string that contains
//! a formatted report on the server's status.
//!
//! \par Server configuration properties
//!
//! \code
//! getpropsresults props=cladmin->getprops();
//!
//! for (std::map<x::property::propvalue, x::property::propvalue>::iterator
//!      b=props->properties.begin(), e=props->properties.end(); b != e; ++b)
//! {
//!     ;
//! }
//!
//! // ...
//!
//! std::pair<getpropsrequest, client::base::request> req=cladmin->getprops_request();
//! \endcode
//!
//! getprops() returns a reference to an object that contains the server's
//! application properties.
//!
//! \code
//! x::property::propvalue name, value;
//!
//! setpropresults res=cladmin->setprop(name, value);
//!
//! if (!res->succeeded)
//!     throw res->errmsg;
//!
//! value=res->newvalue;
//!
//! // ...
//!
//! std::pair<setproprequest, client::base::request> req=setprop_request(name, value);
//! \endcode
//!
//! setprop() sets a server's configuration property. The property name,
//! given by the \c name parameters, must be one of the keys in the map
//! returned by getprops().
//!
//! The object returned by setprop() contains a \c bool succeeded, indicating
//! whether the configuration property was succesfully set. The \c std::string
//! errmsg gives an error message if succeeded is \c false. The newvalue
//! parameter returns the new value of the configuration property. It may
//! differ from the requested \c value parameter, since the property value
//! gets interpreted according to the global system locale.
//!
//! Generally, the new configuration property takes effect immediately, and
//! is permanent. The new configuration property value remains in effect
//! with subsequent object repository server restarts.
//!
//! \code
//! x::property::propvalue name;
//!
//! resetpropresults res=cladmin->resetprop(name);
//!
//! std::cerr << res->resultmsg << std::endl;
//!
//! // ...
//!
//! std::pair<resetproprequest, client::base::request> req=cladmin->resetprop_request(name);
//! \endcode
//!
//! resetprop() resets a configuration property to its default value.
//! The property name,
//! given by the \c name parameters, must be one of the keys in the map
//! returned by getprops().
//!
//! The object returned by resetprop() contains a \c std::string that confirms
//! the request.
//!
//! \note
//! The property's value does not get reset immediately, but takes effect with
//! the next server restart.
//!
//! \par Server status update subscriptions
//!
//! A subset of the server status report can be subscribed to.
//!
//! \code
//! class callbackObj : public serverstatuscallbackObj {
//!
//! public:
//!     callbackObj();
//!     ~callbackObj();
//!
//!    void state(const clusterstate &state);
//!    void serverinfo(const userhelo &serverinfo);
//! };
//!
//! x::ref<callbackObj> callback=x::ref<callbackobj>::create();
//!
//! subscriberserverstatusresult res=client->subscribeserverstatus(callback);
//!
//! req_stat_t status=res->status;
//!
//! x::ref<x::obj> mcguffin=res->mcguffin;
//!
//! x::ref<x::obj> cancel_mcguffin=res->cancel_mcguffin;
//!
//! \endcode
//!
//! The callback object's state() method gets invoked any time the
//! repository cluster's quorum status changes.
//!
//! subscribeserverstatus() installs the callback object, which then begins
//! receiving status updates. subscribeserverstatus() returns two mcguffins.
//! The first mcguffin owns the subscription. The callback object remains
//! installed until the first mcguffin goes out of scope and gets destroyed.
//! Caller should hold on to the mcguffin until the server status subscription
//! is not needed. When it goes out of scope and gets destroyed, the
//! strong reference on the callback object gets released, and the callback
//! object no longer gets status updates.
//!
//! If the connection to the server gets broken, server status updates get
//! stopped without the first mcguffin going out of scope, of first. This
//! situation gets detected using the second mcguffin, the cancel mcguffin.
//! A strong reference on this mcguffin is kept together with the strong
//! reference on the callback object. Both callback objects remain in scope
//! together. If the connection to the server gets broken, the reference on the
//! cancellation mcguffin gets released, together with the callback object's.
//! The discrete cancellation mcguffin avoids the need to set up the callback
//! object to do a double duty of also being a mcguffin.
//!
//! \note
//! When the first mcguffin goes out of scope and gets destroyed, the reference
//! on the cancellation mcguffin also gets released, naturally, together with
//! the callback object's.
//!
//! Its possible that the connection to the server gets dropped immediately,
//! if the server is unavailable. This results in the callback object's, and
//! the mcguffins' tenure to be rather short. This is determined by the status
//! result in the results object. \c req_processed_stat indicates that the
//! subscription has been established. An error of req_disconnected_stat
//! indicates that the connection has been lost. Despite the presence of the
//! cancellation mcguffin, it's likely the last remaining reference to the
//! mcguffin object.
//!
//! \code
//! void callbackObj::state(const clusterstate &state)
//! {
//!   // ...
//! }
//! \endcode
//!
//! The \c state object received containts the following members:
//!
//! - majority, full: two bools, indicating whether
//! the majority of nodes are up, and if all nodes in the cluster are up.
//!
//! - master: the name of the current cluster master.
//!
//! - nodes: a std::set<std::string> giving the names of all the nodes in the
//! cluster.
//!
//! \code
//! void callbackObj::serverinfo(const userhelo &serverinfo)
//! {
//!   // ...
//! }
//! \endcode
//!
//! The serverinfo() callback gets invoked once upon succesful installation of
//! the server status callback object. Most of the time, the callback can
//! expect to have serverinfo() invoked before subscribeserverstatus() returns,
//! or the request mcguffin returned from subscribeserverstatus_request() goes
//! out of scope. However, when subscribeserverstatus() gets invoked immediately
//! after connect_client() or admin_client(), the serverinfo() callback gets
//! invoked only after the server acknowledges the connection attempt, and may
//! never get called if the connection to the server fails.
//!
//! \par Object subscriptions
//!
//! subscribe() installs a callback that gets invoked whenever an object
//! is updated on the server. [USERSUBSCRIBE]
//!
//! \code
//! class handlerObj : public client::base::subscriberObj {
//!
//! public:
//!     handlerObj();
//!     ~handlerObj();
//!     void updated(const std::string &objname, const std::string &suffix);
//! };
//!
//! std::string objname;
//! x::ptr<handlerObj> handler;
//!
//! subscriberesults res=cluser->subscribe(objname, handler);
//!
//! if (res->status != req_processed_stat)
//!     throw x::tostring(res->status);
//!
//! x::ptr<x::obj> mcguffin=res->mcguffin;
//!
//! // ...
//!
//! std::pair<subscriberequest, client::base::request> req=cluser->subscribe_request(objname, handler);
//! \endcode
//!
//! The \c objname parameter given to subscribe() specifies the name of the
//! object to subscribe to. subscribe() installs the handler, whose
//! updated() method will now get invoked any time:
//!
//! - the given object gets added to the object repository
//!
//! - the given object gets removed to the repository
//!
//! - the given object is updated in the repository
//!
//! The parameter to updated() gives the name of the affected object.
//! The same handler object may be installed for multiple object, the
//! parameter to updated() specifies which object the handler gets called for.
//!
//! The \c objname parameter given to subscribe() must be a valid object name.
//! The parameter may end with a trailing slash, such as "jobs/urgent/". This
//! results in the updated() handler getting invoked for any object in this
//! hierarchy, such as "jobs/urgent/00", "jobs/urgent/01", and so forth
//! (including "jobs/urgent/subhier/00"). Passing "" for \c objname installs
//! a handler for any object in the top level hierarchy. [USERSUBSCRIBEHIER]
//!
//! The "suffix" parameter to updated() gets set when subscribed to a hierarchy,
//! and contains the name of the object in the hierarchy that got updated.
//! Appending the suffix to objname gives the full name of the object.
//! [USERSUBSCRIBESUFFIX].
//! The suffix is an empty string when the subscription is for
//! a non-hierarchical object.
//!
//! The object returned by updated() contains a \c status that reports if the
//! callback handler was succesfully installed. There is no explicit
//! unsubscription request. Rather, the object returned by updated() contains
//! a mcguffin representing the subscription. The callback handler remains
//! subscribed for updates to its object until the mcguffin goes out of scope.
//! When the mcguffin gets destroyed, the callback handler gets uninstalled
//! (if the callback handler is in process of being called, to report an
//! appropriate update, when the mcguffin gets destroyed, the callback handler
//! gets uninstalled after it returns). [UNSUBSCRIBE]
//!
//! \note
//! The destruction of the mcguffin does not indicate a completed
//! unsubscription. The destruction of the mcguffin sends a message to a
//! thread that handles the connection with the server, and holds the installed
//! handler. It's possible that the handler might still be invoked immediately
//! after the mcguffin's destruction, before the connection thread processes
//! the unsubscription message.
//!
//! \note
//! The updated() callback gets invoked by the thread that handles the
//! connection with the server. updated() should return quickly, otherwise
//! the connection with the server may be disrupted.
//!
//! There's an upper limit on the maximum number of active subscriptions.
//! Adding a new subscription that exceeds the limit fails with the
//! \c req_toomany_stat error code [SUBSCRIBE2MANY].
//!
//! \note
//! The client may have multiple subscribers for the same object. Each
//! subscription is handled independently, but multiple subscribers for the
//! same object do not count towards the limit, they all collectivity count
//! as one subscription (although each subscriber callback gets notified
//! individually).
//!
//! \par Enumerating contents of an object hierarchy.
//!
//! \code
//! getdirresults res=cluser->getdir("jobs");
//!
//! if (res->status == req_processed_stat)
//! {
//!     for (std::set<std::string>::const_iterator b=res->objects.begin(),
//!          e=res->objects.end(); b != e; ++b)
//!               // ...
//! }
//!
//! // ...
//!
//! std::pair<getdirrequest, client::base::request> req=getdir_request("jobs");
//!
//! \endcode
//!
//! getdir() enumerates all object in an object repository hierarchy.
//! getdir() returns an object with a \c status, indicating whether the
//! request was processed succesfully, and a \c std::set that enumerates
//! all objects in the given hierarchy.
//!
//! The parameter to getdir() must be a valid object name/hierarchy. The
//! above example enumerates all objects in the "jobs" hierarchy, like
//! "jobs/normal" and "jobs/urgent", but not "jobs/special/00".
//! Specifying "" for the hierarchy parameter enumerates objects in the top
//! level hierarchy.
//!
//! Entries with a trailing "/" represent an existing subhierarchy, so
//! "jobs/normal/" indicates that the "jobs/normal" hierarchy exists, with
//! at least one object. If the object repository contains both an
//! "jobs/normal" object, and "jobs/normal/00", the set returned
//! by getdir() contains both "jobs/normal" and "jobs/normal/00".
//!
//! \note
//! Since the object repository can change at any time, by the time the
//! client gets around to looking at the list, the object repository may have
//! been changed.

typedef x::ref<clientObj, clientBase> client;

//! A nullable pointer to a client

typedef x::ptr<clientObj, clientBase> clientptr;

STASHER_NAMESPACE_END

#endif

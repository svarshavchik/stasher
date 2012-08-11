/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_client_H
#define stasher_client_H

/*! \file */

#include <x/ref.H>
#include <x/obj.H>
#include <x/fd.H>
#include <x/uuid.H>
#include <x/mcguffinref.H>

#include <stasher/namespace.H>
#include <stasher/clientfwd.H>
#include <stasher/puttransaction.H>
#include <stasher/contents.H>
#include <stasher/serverstatuscallback.H>
#include <stasher/results.auto.H>
#include <stasher/userhelo.H>

#include <tuple>
#include <set>

//! Object repository API namespace

STASHER_NAMESPACE_START

class clientBase;

//! Reference-counted object for a client connection to an object repository

//! This object is not instantiated directly, but by using
//! \ref clientBase "client::base::connect()" or
//! \ref clientBase "client::base::admin()". See
//! \ref client "client" for more information.
//!
//! \c INSERT_STASHER_NAMESPACE::client and
//! \c INSERT_STASHER_NAMESPACE::clientptr refer to this reference-counted
//! object.

class clientObj : virtual public x::obj {

public:
	class implObj;
private:
	//! Socket connection
	std::string sockname;

	//! A mcguffin-based reference to the implementation thread

	//! \internal

	typedef x::ptr< x::mcguffinrefObj< x::ptr<implObj> > > impl_t;

	//! The implementation thread.

	impl_t impl;

	//! A reference to the connection object thread

	typedef x::ref<implObj> implref;

	//! A nullable reference pointer to the connection object thread

	typedef x::ptr<implObj> implptr;

	//! Retrieve the connection to the execution thread

	//! If the connection has died, try to reconnect. If we can't reconnect
	//! return a null pointer.

	implptr conn() LIBCXX_INTERNAL;

public:
	friend class clientBase;

	//! Constructor

	clientObj(const std::string &socknameArg);

	//! Destructor
	~clientObj() noexcept;

	//! Return client limitations
	userinit getlimits();

	//! Disconnect from the server
	void disconnect();

	//! Shut down the server (admin connections only)
	void shutdown();

	//! Check whether there's a connection with the server

	//! This is just for laughs. Just after this method returns, the
	//! server can go away, and this client object becomes disconnected.
	bool disconnected();

	//! An outstanding request

	class requestObj : virtual public x::obj {

		//! Request's mcguffin

		x::weakptr<x::ptr<x::obj> > reqmcguffin;

	public:
		//! Constructor
		requestObj(//! The request's mcguffin
			   const x::ptr<x::obj> &mcguffinArg);

		//! Destructor
		~requestObj() noexcept;

		//! Return the request's mcguffin

		//! For simplicity's sake, if the request has already been
		//! processed and the mcguffin went out of scope, a dummy
		//! object gets constructed and returned, so when the caller
		//! is done with it, it goes out of scope immediately and
		//! kicks off the caller-installed callbacks.

		x::ref<x::obj> mcguffin();

		//! Check if the request is complete
		bool done();

		//! Wait for the request to complete
		void wait();
	};

	//! A wrapper for puttransactionObj

	class transactionObj : private puttransactionObj {
	public:
		//! Constructor
		transactionObj();

		//! Destructor
		~transactionObj() noexcept;

		using puttransactionObj::newobj;
		using puttransactionObj::updobj;
		using puttransactionObj::delobj;
		using puttransactionObj::admin;

		friend class x::ptr<transactionObj>;
		friend class x::const_ptr<transactionObj>;
		friend class x::ptrImpl<transactionObj>;
		friend class clientObj;

		friend class x::const_ptr<puttransactionObj>;
	};

	//! A reference-counted parameter to get()

	class getreqObj : virtual public x::obj {

	public:
		//! Return uuids of these objects

		std::set<std::string> objects;

		//! Open the objects, in addition to returning their uuids

		//! This defaults to false.
		//!
		bool openobjects;

		//! Access objects directly, even if not in quorum

		//! Administrative connection required.
		//!
		bool admin;

		getreqObj();
		~getreqObj() noexcept;
	};

	//! A subscriber for object updates

	//! Refer to this class as \c client::base::subscriberObj

	class subscriberObj : virtual public x::obj {

	public:
		//! Constructor
		subscriberObj();

		//! Destructor
		~subscriberObj() noexcept;

		//! The object has been updated
		virtual void updated(//! Name of the object
				     const std::string &objname,

				     //! If the object is a directory, the object in the directory that was added, removed, or changed.
				     const std::string &suffix)=0;
	};

	//! Return uuids of objects on the server

	//! This is an overloaded version that assembles the contents request
	//! on the fly.

	getresults get(const std::set<std::string> &objnames,
		       bool openobjects=false,
		       bool admin=false);

	//! Return connection limits

	userhelo gethelo();

#include <stasher/client.api.auto.H>

	//! For debugging purposes
	void debugSetLimits(const userinit &fakelimits);

	//! For debugging purposes
	void debugWaitDisconnection();

	//! Error message -- lost connection with server

	static const char lost_connection_errmsg[] LIBCXX_HIDDEN;

	//! Return uuids of objects on the server

	//! This is an overloaded variadic version, for convenience.

	template<typename ...Args>
	getresults get(const std::string &objname,
		       Args && ...args)
	{
		x::ref<getreqObj> req(x::ref<getreqObj>::create());

		req->objects.insert(objname);
		return get_variadic(req, std::forward<Args>(args)...);
	}

private:
	//! Implementation of the variadic get().

	//! Consumed all arguments.
	//!
	getresults get_variadic(x::ref<getreqObj> &req)
	{
		return get(req);
	}

	//! Implementation of the variadic get().

	//! The bool openobject parameter got specified

	getresults get_variadic(x::ref<getreqObj> &req,
				bool openobjects,
				bool admin=false)
	{
		req->openobjects=openobjects;
		req->admin=admin;
		return get(req);
	}

	//! Implementation of the variadic get().

	//! The next object name is specified.

	template<typename ...Args>
	getresults get_variadic(x::ref<getreqObj> &req,
				const std::string &objname,
				Args && ...args)
	{
		req->objects.insert(objname);
		return get_variadic(req, std::forward<Args>(args)...);
	}
};

//! Refer to this class as \c INSERT_STASHER_NAMESPACE::client::base or \c INSERT_STASHER_NAMESPACE::clientptr::base.

//! This is the base class for \ref client "client" and
//! \ref clientptr "clientptr".

class clientBase : public x::ptrrefBase {

public:
	friend class clientObj;

	//! Return all node directories installed in the default location.

	//! The default location for node directories gets set up at
	//! installation time. 
	static void defaultnodes(std::set<std::string> &paths);

	//! Client connection to an object repository

	//! Returns a \ref client "handle" for a connection object.
	//! An exception gets thrown if a server is not running.

	static client connect(//! Object repository directory
			      const std::string &dir);

	//! Connect to the default object repository

	//! Call defaultnodes(). If it does not return exactly one repository
	//! node directory, throw an exception. Else, connect to it.

	static client connect();

	//! Admin connection to an object repository

	//! Returns a \ref client "handle" for a connection object.
	//! An exception gets thrown if a server is not running.
	static client admin(const std::string &dir);

	//! Create a client handle for a connection to an object repository

	//! This is like connect(), except that it creates the handle but does
	//! not connect to the server. The connection gets made automatically
	//! with the first request. A connection failure does not result in a
	//! thrown exception. The corresponding request fails with an
	//! appropriate error code. The next request makes another connection
	//! attempt.
	static client connect_client(const std::string &dir);

	//! Create a client handle for an administrative connection to an object repository

	//! This is like connect_client(), except that the handle makes
	//! an administrative connection to the object repository.
	static client admin_client(const std::string &dir);


	//! Internal creation function

	//! \internal

	static client create_handle(const std::string &) LIBCXX_INTERNAL;

	//! Results of a connection request

	class connstatusObj : virtual public x::obj {

	public:
		connstatusObj();
		~connstatusObj() noexcept;

		//! The connection request's status
		req_stat_t result;
	};

	//! A connection request reference

	typedef x::ref<connstatusObj> connstatus;

	//! A connection request nullable reference pointer.

	typedef x::ptr<connstatusObj> connstatusptr;

	//! A transaction

	typedef x::ref<clientObj::transactionObj> transaction;

	//! A nullable reference pointer to a transaction

	typedef x::ptr<clientObj::transactionObj> transactionptr;

	//! Results of the transaction

	typedef x::ref<putresultsObj> putresults;

	//! A nullable reference pointer to results of the transaction

	typedef x::ptr<putresultsObj> putresultsptr;

	//! A reference to a request passed to get().

	typedef x::ref<clientObj::getreqObj> getreq;

	//! A nullable reference pointer to a request passed to get().

	typedef x::ptr<clientObj::getreqObj> getreqptr;

	//! A reference to a pending request
	typedef x::ref<clientObj::requestObj> request;

	//! A nullable pointer reference to a pending request
	typedef x::ptr<clientObj::requestObj> requestptr;

	//! A client::subscriberObj alias

	typedef clientObj::subscriberObj subscriberObj;

	//! A reference to a subscriberObj

	typedef x::ref<subscriberObj> subscriber;

	//! A nullable reference pointer to a subcsriberObj

	typedef x::ptr<subscriberObj> subscriberptr;

	//! client::base::clusterconfig is the object in the repository where the cluster configuration is read from

	static const char clusterconfigobj[];

	//! client::base::maxobjects is the object in the repository that stores the maximum number of client objects per transaction

	static const char maxobjects[];

	//! client::base::maxobjectsize is the object in the repository that stores the maximum aggregate object size updated by a transaction

	static const char maxobjectsize[];

private:

	//! Internal connection function

	static void connect_socket(const client &) LIBCXX_INTERNAL;

	//! The tuple returned by do_connect

	//! \internal

	typedef std::tuple<clientObj::implref,
			   request, connstatus> do_connect_t;

	//! Internal connection function

	static do_connect_t do_connect(const client &cl) LIBCXX_INTERNAL;
};

STASHER_NAMESPACE_END

#endif

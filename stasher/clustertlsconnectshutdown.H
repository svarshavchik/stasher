/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef clustertlsconnectshutdown_H
#define clustertlsconnectshutdown_H

#include <x/threads/run.H>
#include <x/logger.H>
#include <x/fd.H>
#include <x/gnutls/session.H>
#include <x/eventqueuemsgdispatcher.H>
#include <x/options.H>

#include "stasher/namespace.H"
#include "stoppablethreadtrackerfwd.H"

//! Shut down a connection to a peer that uses TLS

//! Implement an orderly TLS shutdown when a connection to a peer that uses
//! TLS terminates.
//!
//! When the connection execution thread terminates, this thread gets started.
//! The thread terminates the TLS connection, and exits.

class clustertlsconnectshutdownObj
	: virtual public x::eventqueuemsgdispatcherObj,
	  public x::runthreadname {

	//! Logging

	LOG_CLASS_SCOPE;

	//! Connection timeout.

	static x::property::value<unsigned> bye_timeout;

	//! Return the effective bye timeout

	virtual unsigned getTimeout();

	//! Return the thread's name

	std::string getName() const;

public:

	//! A destructor callback for TLS connection objects

	//! When the entire connection with the peer gets TLS-encrypted,
	//! and not just the authentication phase, this destructor callback
	//! gets attached to the mcguffin held by the connection object.
	//!
	//! The destructor callback starts a separate thread that invokes
	//! gnutls_bye(), to orderly shut down the connection.

	class mcguffin_destructor_cb : public x::destroyCallbackObj {

	public:
		//! The tracker that starts the shutdown thread.
		STASHER_NAMESPACE::stoppableThreadTracker tracker;

		//! The shutdown thread
		x::ref<clustertlsconnectshutdownObj> shutdown;

		//! Socket connection
		x::fd socket;

		//! GnuTLS session
		x::gnutls::session session;

		//! Constructor
		mcguffin_destructor_cb(const STASHER_NAMESPACE::stoppableThreadTracker
				       &trackerArg,
				       const x::ref<clustertlsconnectshutdownObj
						    > &shutdownArg,
				       const x::fd &socketArg,
				       const x::gnutls::session &sessionArg);


		//! Destructor
		~mcguffin_destructor_cb() noexcept;

		//! Destructor callback
		void destroyed() noexcept;
	};


public:
	//! Constructor
	clustertlsconnectshutdownObj();

	//! Destructor
	~clustertlsconnectshutdownObj() noexcept;

	//! Create a destructor callback that starts a TLS shutdown thread

	//! A TLS shutdown thread object gets instantiated, and will be started
	//! when a mcguffin goes out of scope.
	//!
	//! The mcguffin argument is typically passed to
	//! repopeerconnectionobj::run(), and then the resulting
	//! object passed as the starting argument to the execution thread.
	//!
	//! When the peer connection object execution thread terminates, the
	//! mcguffin gets destroyed. create() attaches a destructor callback
	//! to the mcguffin that starts an execution thread, implemented by
	//! this class, that shuts down the TLS session.

	static void create(//! The connection mcguffin
			   const x::ptr<x::obj> &connection_mcguffin,

			   //! The new thread gets started via this tracker
			   const x::ptr<STASHER_NAMESPACE::stoppableThreadTrackerObj> &tracker,

			   //! Connection
			   const x::fd &socket,

			   //! The TLS session. If it's NULL, create() quietly
			   //! returns without doing anything.
			   const x::gnutls::session &session,

			   //! The shutdown thread
			   const x::ptr<clustertlsconnectshutdownObj> &thr);


	//! Main execution thread (virtual override in regression tests)

	virtual void run(const x::fd &socket,
			 const x::gnutls::session &session);
};

#endif

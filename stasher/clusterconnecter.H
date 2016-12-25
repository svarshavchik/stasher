/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef clusterconnecter_H
#define clusterconnecter_H

#include <x/threads/run.H>
#include <x/logger.H>
#include <x/options.H>
#include <x/fd.H>
#include <x/epoll.H>
#include <x/fdtimeout.H>
#include <x/fditer.H>
#include <x/timerfd.H>
#include <x/eventfd.H>
#include <x/gnutls/session.H>

#include <x/stoppable.H>
#include "nodeclusterstatus.H"
#include "tobjrepo.H"
#include "clusterinfofwd.H"
#include "clusterlistenerfwd.H"
#include "stasher/namespace.H"

class clusterconnecterObj;
class trandistributorObj;

STASHER_NAMESPACE_START
class nodeinfo;
STASHER_NAMESPACE_END

//! Set up a connection with another peer in the cluster

//! This thread makes a connection, exchanges the current cluster status
//! with the peer, and determines whether encryption gets used for the entire
//! session.
//!
//! On the client side, the peer's "host" option names peer's host. A socket
//! connection attempt is made to the peer running on the host.
//!
//! On the server side, the socket has already been accepted by
//! clusterlistenerObj. The socket passed to clusterlistenerObj::start_network()
//! is handed over here, to figure out which peer it's from.
//!
//! \par Starting the thread
//!
//! run() takes five parameters: a clusterinfo object, a clusterlistener
//! object, then either a socket or a peer name, the repository object,
//! and the distributor object.
//!
//! In server mode, the third parameter is a connected file descriptor. In
//! client mode, the third parameter is the name of the peer to connect
//! to.
//!
//! Once a connection has been negotiated, connected() gets invoked, which
//! must be implemented in the subclass. connected() receives the following
//! parameters:
//!
//! - peer's name
//!
//! - file descriptor object.
//!
//! - a TLS session object reference, if the entire connection should be
//! encrypted.
//!
//! - an input iterator. There could be some stuff already buffered, after
//! the initial handshake completed, if the peer was chatty. If a TLS session
//! remains for the duration of the connection, the input iterator reads from
//! the TLS session, otherwise it reads from the timeout file descriptor
//! transport.
//!
//! - peer's status, which is the clusterinfo's status at the time the peer
//! invoked create_client() or create_server() on its side.
//!
//! - the official timestamp of this connection. Both the client and the server
//! side calls to connected() will have the same timestamp.
//!
//! - this connection's uuid. Both the client and the server side calls to
//! connected() will have the same uuid.
//!
//! - the listener object.
//!

class clusterconnecterObj : public x::stoppableObj,
			    public x::runthreadname {

	//! The logger object

	LOG_CLASS_SCOPE;

	//! This thread's name

	std::string name;

protected:

	//! Event file descriptor used to stop the thread

	x::eventfd terminate_eventfd;
public:
	//! Constructor
	clusterconnecterObj(//! This thread's name
			    const std::string &name);

	//! Destructor
	~clusterconnecterObj() noexcept;

	//! Return this thread's name.
	std::string getName() const;

	//! Terminate this thread

	void stop();

private:

	//! Connection timeout.

	static x::property::value<unsigned> connect_timeout;

	//! For debugging purposes -- disable TLS for negotiation

	//! Do not use, except in regression tests/manual debugging

	static x::property::value<bool> debugnotls;

	//! Callback, connected

	virtual void connected(//! Peer's name
			       const std::string &peername,

			       //! File descriptor for the socket
			       const x::fd &socket,

			       //! TLS session, if it's an encrypted one
			       const x::gnutls::sessionptr &session,

			       //! Input iterator
			       const x::fd::base::inputiter &inputiter,

			       //! Peer's status
			       const nodeclusterstatus &peerstatus,

			       //! Connection timestamp
			       time_t connectTimestamp,

			       //! Connection uuid
			       const x::uuid &connuuid,

			       //! The listener object
			       const clusterlistener &listener)=0;

	//! Check for node status conflicts

	void check_conflict(const nodeclusterstatus &my_status,
			    const nodeclusterstatus &peer_status,
			    const std::string &peername,
			    const clusterlistener &listener);


	class dummyCallbackObj;

	//! Common initialization for both client and server connections.

	class connect_common {

	public:
		//! Connect timeout
		x::timerfd timer;

		//! Epoll file descriptor
		x::epoll epollfd;

		//! Timeout callback object
		x::ref<dummyCallbackObj> epollcb;

		connect_common(const x::eventfd &eventfd);
		~connect_common() noexcept;
	};

	//! We will connect to another peer

	void connect(//! Common stuff to client and server connections
		     connect_common &common,

		     //! Our cluster info
		     const clusterinfo &cluster,

		     //! Listener info
		     const clusterlistener &listener,

		     //! Which peer to connect to
		     const std::string &peername,

		     //! Object repository
		     const tobjrepo &repo,

		     //! The object distributor
		     const x::ptr<trandistributorObj> &distributor);

	//! We're accepting a connection from another peer

	void connect(//! Common stuff to client and server connections
		     connect_common &common,

		     //! Our cluster info
		     const clusterinfo &cluster,

		     //! Listener info
		     const clusterlistener &listener,

		     //! Connection object
		     const x::fd &fd,

		     //! Object repository
		     const tobjrepo &repo,

		     //! The object distributor
		     const x::ptr<trandistributorObj> &distributor);

	//! Establish a connection
	x::fd do_connect(connect_common &common,
			 const clusterinfo &cluster,
			 const std::string &peername,
			 STASHER_NAMESPACE::nodeinfo &peernodeinfo);


	class installPeers;

public:
	friend class socktimeout;

	//! The thread

	template<typename ...Args>
	void run(Args && ...args)
	{
		x::logger::context context(getName());

		try {
			connect_common common(terminate_eventfd);

			connect(common, std::forward<Args>(args)...);

		} catch (const x::exception &e)
		{
			LOG_ERROR(e);
		}
	}
};

#endif

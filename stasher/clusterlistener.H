/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef clusterlistener_H
#define clusterlistener_H

#undef libx_autogen_suppress_inner

#include <x/threadmsgdispatcher.H>
#include <x/threads/run.H>
#include <x/httportmap.H>
#include <x/gnutls/credentials.H>
#include <x/logger.H>
#include <x/options.H>
#include <x/ymd.H>
#include <x/property_properties.H>

#include "threadreportimpl2.H"
#include "clusterlistenerfwd.H"

//! A thread that listens and accept cluster connections

//! Instantiating this object reads the node's certificate from the
//! node key directory, creates a listening port, registers the port
//! with the portmapper, using the certificate's name, and creates two
//! listening sockets "privsocket" and "pubsocket",
//! with privsocket's permissions set to 0700 and pubsocket's permissions set to
//! 0777, then creates a filesystem domain socket named "socket", in each
//! directory. [INIT]
//!
//! If the portmapper port is already registered, an exception gets thrown,
//! and the node key directory remains untouched. [PORTLOCK]
//!
//! The listener thread is started by invoking start(). The thread startup
//! argument is not used for anything.
//!
//! credentials() returns a credentials object pre-populated with the node
//! certificate and the root certificate that was loaded from the
//! node key directory. [CREDENTIALS]
//!
//! If the constructor finds that new cluster certificates are present
//! (put in there by repomg::export_key_local(), they get automatically moved
//! into the object repository. When this nodes joins the cluster, if it turns
//! out that this is the first node in the cluster, this results in the updated
//! cluster certificates getting bootstrapped and distributed to other nodes.
//! If this node becomes someone else's slave, its object repository will get
//! resynced, of course, and the cluster's official certificate list will
//! supercede [CREDENTIALSUPDATE].
//!
//! reload() reloads the node's certificate from the node key directory,
//! updating the object returned from future calls to credentials(). An error
//! gets logged if the new certificate's name is different than the existing
//! certificate's, and the new certificate does not get loaded. [RELOAD]
//!
//! check_expiration() throws an exception
//! if the node certificate's expiration time
//! is less than 30 days from the current time. [CHECK]
//!
//! This is a stoppable thread. The thread, once started, listens and
//! accepts connections from the listening sockets. An accepted connection
//! results in network(), socket, or pubsocket() virtual function invocation.
//! These functions must be defined by the subclass to take the appropriate
//! action with the file descriptor, and return immediately. [CONNECTIONS]
//!
//! This is a one-time use class.

class clusterlistenerObj : virtual public x::threadmsgdispatcherObj,
			   public x::runthreadname,
			   public STASHER_NAMESPACE::threadreportimpl2Obj {

#include "clusterlistener.msgs.H"

protected:
	//! The logger object

	LOG_CLASS_SCOPE;

private:
	//! Node directory
	std::string directory;

	//! Connection to the portmapper

	//! Once the thread starts, this is moved to the executing thread's
	//! stack, to make sure that the listener gets deregistered with the
	//! portmapper before it terminates.
	x::httportmapptr portmapper;

	//! Property - warning interval for an expiring certificate
	static x::property::value<x::ymd::interval> certwarntime;

	//! Property - error interval for an expiring certificate
	static x::property::value<x::ymd::interval> certerrtime;

	//! TLS credentials
	x::gnutls::credentials::certificateptr cred;

	//! This node's certificate, that's installed in the credentials object.
	x::gnutls::x509::crt cert;

	//! The cluster certificates, that are installed in the credentials obj.
	std::list<x::gnutls::x509::crt> clustcerts;

	//! List of network sockets.

	//! \internal
	//! networksocks must be declared after TLS parameters, above.
	//! Dependency on the initialization order.

	class networksocks_t : public std::list<x::fd> {

	public:
		networksocks_t(clusterlistenerObj *p);
		~networksocks_t() noexcept;
	};

	//! Network socket listeners
	networksocks_t networksocks;

	//! Private socket listener
	x::fd privsock;

	//! Public socket listener
	x::fd pubsock;

	static x::fd init_sock(const std::string &directory,
			       mode_t mode,
			       const std::string &sockname);

	class epollcb;

	//! New network socket connection
	void accept_network(const x::fd &sock);

	//! New private socket connection
	void accept_privsock(const x::fd &sock);

	//! New public socket connection
	void accept_pubsock(const x::fd &sock);

	//! Event file descriptor is readable
	void tickle_eventfd(const x::fd &sock);

	//! Handle a network connection
	virtual void start_network(const x::fd &sock,
				   const x::sockaddr &addr)
=0;

	//! Handle a private socket connection
	virtual void start_privsock(const x::fd &sock,
				    const x::sockaddr &addr)=0;

	//! Handle a public socket connection
	virtual void start_pubsock(const x::fd &sock,
				    const x::sockaddr &addr)=0;

public:
	friend class networksocks_t;

	//! Constructor
	clusterlistenerObj(const std::string &directoryArg);


	//! Destructor
	~clusterlistenerObj() noexcept;

	//! Return this thread's name

	std::string getName() const;

	//! Return this node's name, from the certificate

	std::string nodeName() const noexcept;

	//! Return this cluster's name, from the certificate
	std::string clusterName() const noexcept;

	//! Return the cached node TLS credentials

	x::gnutls::credentials::certificate credentials();

	//! Reload node's certificate and private key.
	void reload();

	//! Install a new certificate, and reload it.

	//! Returns true if the certificate was installed, false if the
	//! certificate's name is different than the existing cert.

	bool install(const std::string &certificate);

	//! Execution thread
	void run(msgqueue_auto &q);

protected:

	//! Reload credentials
	void reload_internal();

private:
	//! Report this thread's status
	std::string report(std::ostream &rep);
};

#endif

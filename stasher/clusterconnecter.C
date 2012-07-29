/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "clusterconnecter.H"
#include "clusterinfo.H"
#include "clusterconnecter.H"
#include "clusterlistener.H"
#include "repoclusterinfo.H"
#include "trandistributor.H"
#include "trancommit.H"
#include "stasher/client.H"
#include <x/timespec.H>
#include <x/fditer.H>
#include <x/epoll.H>
#include <x/timerfd.H>
#include <x/serialize.H>
#include <x/deserialize.H>
#include <x/httportmap.H>
#include <x/destroycallbackflag.H>

LOG_CLASS_INIT(clusterconnecterObj);

x::property::value<unsigned>
clusterconnecterObj::connect_timeout(L"connecttimeout", 30);

x::property::value<bool> clusterconnecterObj::debugnotls(L"debugnotls", false);

clusterconnecterObj::clusterconnecterObj(const std::string &nameArg)

	: name(nameArg), terminate_eventfd(x::eventfd::create())
{
}

clusterconnecterObj::~clusterconnecterObj() noexcept
{
}

std::string clusterconnecterObj::getName() const
{
	return name;
}

void clusterconnecterObj::stop()
{
	terminate_eventfd->event(1);
}

//! epoll callback installed for the timeout and stop file descriptors

//! Records the fact that the file descriptor became readable. This is used
//! to determine how exceptions get handled. When connecting to another peer,
//! if the terminator file descriptor or the timeout file descriptor becomes
//! readable, the exception gets propagated in order to abort the thread.
//! Any other exception is presumably from the failed connection attempt to
//! the peer. If the peer has multiple hostnames, a connection attempt is
//! made to the next hostname.

class clusterconnecterObj::dummyCallbackObj : public x::epoll::base::callbackObj {

public:
	dummyCallbackObj();
	~dummyCallbackObj() noexcept;

	bool timedout;

	void event(const x::fd &filedesc, event_t events);
};

clusterconnecterObj::dummyCallbackObj::dummyCallbackObj()
	: timedout(false)
{
}

clusterconnecterObj::dummyCallbackObj::~dummyCallbackObj() noexcept
{
}

void clusterconnecterObj::dummyCallbackObj::event(const x::fd &filedesc,
						  event_t events)

{
	timedout=true;
}

class clusterconnecterObj::installPeers
	: public trandistributorObj::internalTransactionObj {

public:
	tobjrepo repo;
	STASHER_NAMESPACE::nodeinfomap nodes;

	installPeers(const tobjrepo &repoArg) : repo(repoArg)
	{
	}

	~installPeers() noexcept
	{
	}

	void doit()
	{
		LOG_DEBUG("Installing new cluster node map");

		newtran tr(repo->newtransaction());

		x::fd fd(tr->newobj(STASHER_NAMESPACE::client
				    ::base::clusterconfigobj));

		{
			x::fd::base::outputiter fditer(fd);

			x::serialize
				::iterator<x::fd::base::outputiter>
				ser_iter(fditer);

			ser_iter(nodes);

			fditer.flush();
		}
		fd->close();

		x::eventfd eventfd(x::eventfd::create());

		x::uuid uuid=tr->finalize();

		trancommit tran(repo->begin_commit(uuid, eventfd));

		LOG_TRACE("Waiting for transaction to become ready");

		while (!tran->ready())
			eventfd->event();

		if (tran->verify())
		{
			tran->commit();
			LOG_TRACE("Commited a new cluster node map");
		}
		else
		{
			LOG_TRACE("Unable to install a new cluster node map");
		}

		repo->cancel(uuid);
		LOG_TRACE("Completed installation of a new cluster node map");
	}
};

clusterconnecterObj::connect_common::connect_common(const x::eventfd &eventfd)
	: timer(x::timerfd::create(CLOCK_MONOTONIC)),
	  epollfd(x::epoll::create()),
	  epollcb(x::ref<dummyCallbackObj>::create())
{
	timer->set(0, connect_timeout.getValue());
	epollfd->nonblock(true);
	timer->epoll_add(EPOLLIN, epollfd, epollcb);

	eventfd->epoll_add(EPOLLIN, epollfd, epollcb);
}

clusterconnecterObj::connect_common::~connect_common() noexcept
{
}

void clusterconnecterObj::connect(connect_common &common,
				  const clusterinfo &cluster,
				  const clusterlistener &listener,
				  const x::fd &socket,
				  const tobjrepo &repo,
				  const x::ptr<trandistributorObj> &distributor)
{
#ifdef CLUSTERCONNECTED_CONNECT_SERVER_INIT_HOOK
	CLUSTERCONNECTED_CONNECT_SERVER_INIT_HOOK(arg);
#endif

	// Incoming connection.

	socket->nonblock(true);
	socket->keepalive(true);

	// Have the TLS session use the timeout-armed transport, which
	// uses the epoll file descriptor as the timeout trigger.

	x::fdtimeout timeout(x::fdtimeout::create(socket));

	timeout->set_terminate_fd(common.epollfd);

	x::gnutls::sessionptr sess;
	std::string peername;

	if (debugnotls.getValue())
	{
		{
			x::fd::base::inputiter in_iter(timeout), end_iter;

			x::deserialize::iterator<x::fd::base::inputiter>
				deserialize_iter(in_iter, end_iter);
			deserialize_iter(peername);

			x::fd::base::outputiter out_iter(timeout,
							 x::fd::obj_type
							 ::get_buffer_size());

			*out_iter++ = '\0';
			out_iter.flush();
		}
	}
	else
	{
		sess=x::gnutls::session::create(GNUTLS_SERVER, timeout);

		sess->set_default_priority();
		sess->server_set_certificate_request();
		sess->credentials_set(listener->credentials());

		int dummy;

		if (!sess->handshake(dummy))
			throw EXCEPTION("TLS handshake failed");

		// Figure out who the peer is.
		std::list<x::gnutls::x509::crt> crts;

		sess->get_peer_certificates(crts);

		if (crts.empty())
			throw EXCEPTION("No certificate after TLS handshake");

		peername=crts.front()->get_dn(GNUTLS_OID_X520_COMMON_NAME);

		sess->verify_peer(peername);
	}

	LOG_INFO(cluster->getthisnodename()
		 << ": Connection from " << peername);

	x::fd::base::inputiter in_iter, end_iter;

	size_t bufsize;
	x::fdbaseptr src;

	if (!sess.null())
	{
		src=sess;
		bufsize=sess->get_max_size();
		in_iter=x::fd::base::inputiter(sess, bufsize);
	}
	else
	{
		src=timeout;
		bufsize=x::fd::obj_type::get_buffer_size();
		in_iter=x::fd::base::inputiter(src, bufsize);
	}

	{
		x::deserialize::iterator<x::fd::base::inputiter>
			deserialize_iter(in_iter, end_iter);

		x::ptr<installPeers> install_peers=
			x::ptr<installPeers>::create(repo);

		LOG_TRACE(cluster->getthisnodename()
			  << ": Connection from " << peername << ": "
			  << "deserializing cluster node list");

		deserialize_iter(install_peers->nodes);

		LOG_TRACE(cluster->getthisnodename()
			  << ": Connection from " << peername << ": "
			  << "deserialized cluster node list");

		// No need to spin on the internal transaction, unless
		// absolutely necessary.

		if (cluster->empty())
		{
			// New client: swipe cluster's node configuration

			LOG_INFO("Importing cluster configuration from "
				 << peername);

			distributor->internal_transaction(install_peers);

			auto destroy_cb(x::destroyCallbackFlag::create());

			install_peers->addOnDestroy(destroy_cb);

			install_peers=x::ptr<installPeers>();

			destroy_cb->wait();

			LOG_INFO("Imported cluster configuration from "
				 << peername);

			// Even though this is not atomic, and some other
			// peer in the stampeding herd that's connecting to this
			// new node might get in here first, that's ok because
			// of the newtransaction, which will
			// fail if someone else in the herd gets ahead of us.
		}
	}

	STASHER_NAMESPACE::nodeinfo peernodeinfo;

	cluster->getnodeinfo(peername, peernodeinfo);

	nodeclusterstatus my_status(*clusterinfoObj::status(cluster));

	x::uuid connuuid;

	// Send current cluster status to the peer.

	{
		x::fd::base::outputiter out_iter(src, bufsize);

		bool useencryption=peernodeinfo.useencryption();

		x::serialize::iterator<x::fd::base::outputiter>
			serialize_iter(out_iter);

		serialize_iter(useencryption);
		serialize_iter(my_status);
		serialize_iter(connuuid);
		out_iter.flush();
	}

	LOG_TRACE(cluster->getthisnodename()
		  << ": Connection from " << peername << ": "
		  << "sent my status");

	// Retrieve the peer's status

	bool useencryption;
	nodeclusterstatus peer_status;
	time_t timestamp;

	{
		x::deserialize::iterator<x::fd::base::inputiter>
			deserialize_iter(in_iter, end_iter);

		deserialize_iter(useencryption);
		deserialize_iter(peer_status);
		deserialize_iter(timestamp);

		// If we'll continue to use encryption, reset the TLS session
		// object to read/write directly from the socket. This
		// disconnects the timeout wrapper.
		if (useencryption && !sess.null())
			sess->setTransport(socket);
		else
		{
			// Otherwise, terminate the TLS session.

			int dummy;

			if (!sess.null() && !sess->bye(dummy))
				throw EXCEPTION("TLS bye failed");
			sess=x::gnutls::sessionptr();
			in_iter=x::fd::base::inputiter(socket,
						       x::fd::obj_type
						       ::get_buffer_size());
		}
	}

	LOG_TRACE(cluster->getthisnodename()
		  << ": Connection from " << peername << ": "
		  << "received peer's status");

	check_conflict(my_status, peer_status, peername, listener);

	LOG_DEBUG(cluster->getthisnodename()
		  << ": Connection from " << peername << " complete");

	connected(peername, socket, sess, in_iter, peer_status, timestamp,
		  connuuid, listener);
}

void clusterconnecterObj::connect(connect_common &common,
				  const clusterinfo &cluster,
				  const clusterlistener &listener,
				  const std::string &peername,
				  const tobjrepo &repo,
				  const x::ptr<trandistributorObj> &distributor)
{
#ifdef CLUSTERCONNECTED_CONNECT_CLIENT_INIT_HOOK
	CLUSTERCONNECTED_CONNECT_CLIENT_INIT_HOOK(arg);
#endif

	STASHER_NAMESPACE::nodeinfo peernodeinfo;
	x::fdptr socket;

	time_t timestamp=time(NULL);

	try {
		socket=do_connect(common, cluster, peername, peernodeinfo);
	} catch (const x::exception &e)
	{
		LOG_ERROR("Connect: " << peername << ": " << e);
		return;
	}

	LOG_DEBUG(cluster->getthisnodename()
		  << ": Connection to " << peername << ": "
		  << "setting up connection");

	socket->nonblock(true);
	socket->keepalive(true);

	x::fdtimeout timeout(x::fdtimeout::create(socket));

	timeout->set_terminate_fd(common.epollfd);

	x::gnutls::sessionptr sess;

	std::string connected_peername;

	if (debugnotls.getValue())
	{
		connected_peername=peername;

		x::fd::base::outputiter out_iter(timeout,
						 x::fd::obj_type::
						 get_buffer_size());

		x::serialize::iterator<x::fd::base::outputiter>
			serialize_iter(out_iter);

		std::string me=listener->nodeName();
		serialize_iter(me);
		out_iter.flush();

		x::fd::base::inputiter in_iter(timeout,
					       x::fd::obj_type
					       ::get_buffer_size());

		(void)*in_iter++;
	}
	else
	{
		sess=x::gnutls::session::create(GNUTLS_CLIENT, timeout);
		sess->set_default_priority();
		sess->credentials_set(listener->credentials());

		int dummy;

		if (!sess->handshake(dummy))
			throw EXCEPTION("TLS handshake failed");

		std::list<x::gnutls::x509::crt> crts;

		sess->get_peer_certificates(crts);
		sess->verify_peer(peername);

		if (crts.empty())
			throw EXCEPTION("No certificate after TLS handshake");

		connected_peername=
			crts.front()->get_dn(GNUTLS_OID_X520_COMMON_NAME);
	}

	if (peername != connected_peername)
	{
		LOG_FATAL("Connected to " << peername
			  << ", but it identifies itself as "
			  << connected_peername);
		throw EXCEPTION("Connected to the wrong peer");
	}

	LOG_INFO(cluster->getthisnodename()
		 << ": Connected to " << peername);

	x::fd::base::inputiter in_iter, end_iter;
	x::fd::base::outputiter out_iter;

	if (sess.null())
	{
		in_iter=x::fd::base::inputiter(timeout,
					       x::fd::obj_type::get_buffer_size());
		out_iter=x::fd::base::outputiter(timeout,
						 x::fd::obj_type::get_buffer_size()
						 );
	}
	else
	{
		in_iter=x::fd::base::inputiter(sess, sess->get_max_size());
		out_iter=x::fd::base::outputiter(sess, sess->get_max_size());
	}

	x::serialize::iterator<x::fd::base::outputiter>
		serialize_iter(out_iter);

	{
		STASHER_NAMESPACE::nodeinfomap
			nodes(repoclusterinfoObj::loadclusterinfo(repo));

		serialize_iter(nodes);
		out_iter.flush();
	}

	LOG_TRACE(cluster->getthisnodename()
		  << ": Connection to " << peername << ": "
		  << "sent cluster list");

	bool useencryption;
	nodeclusterstatus peer_status;
	nodeclusterstatus my_status(*clusterinfoObj::status(cluster));
	x::uuid connuuid;

	{
		x::deserialize::iterator<x::fd::base::inputiter>
			deserialize_iter(in_iter, end_iter);

		deserialize_iter(useencryption);
		deserialize_iter(peer_status);
		deserialize_iter(connuuid);
	}

	if (peernodeinfo.useencryption() || useencryption)
		useencryption=true;

	if (sess.null())
		useencryption=false;

	LOG_TRACE(cluster->getthisnodename()
		  << ": Connection to " << peername << ": "
		  << "received peer's status");

	serialize_iter(useencryption);
	serialize_iter(my_status);
	serialize_iter(timestamp);
	out_iter.flush();

	LOG_TRACE(cluster->getthisnodename()
		  << ": Connection to " << peername << ": "
		  << "sent my status");

	if (useencryption)
		sess->setTransport(socket);
	else
	{
		int dummy;

		if (!sess.null() && !sess->bye(dummy))
			throw EXCEPTION("TLS bye failed");
		sess=x::gnutls::sessionptr();
		in_iter=x::fd::base::inputiter(socket,
					       x::fd::obj_type::get_buffer_size());
	}

	check_conflict(my_status, peer_status, peername, listener);

	LOG_DEBUG(cluster->getthisnodename()
		  << ": Connection to " << peername
		  << " complete");

	connected(peername, socket, sess, in_iter, peer_status, timestamp,
		  connuuid, listener);
}

void clusterconnecterObj::check_conflict(const nodeclusterstatus &my_status,
					 const nodeclusterstatus &peer_status,
					 const std::string &peername,
					 const clusterlistener &listener)
{
	std::string me=listener->nodeName();

	if (my_status.master == peername)
	{
		LOG_ERROR(me << ": duplicate connection with " << peername
			  << " detected: this node already peer's master");
	}
	else if (peer_status.master == me)
	{
		LOG_ERROR(me << ": duplicate connection with " << peername
			  << " detected: peer already this node's master");
	}
	else return;

	throw EXCEPTION(me + ": duplicate connection with " + peername);
}

x::fd clusterconnecterObj::do_connect(connect_common &common,
				      const clusterinfo &cluster,
				      const std::string &peername,
				      STASHER_NAMESPACE::nodeinfo &peernodeinfo)

{
	cluster->getnodeinfo(peername, peernodeinfo);

	for (std::pair<STASHER_NAMESPACE::nodeinfo::options_t::iterator,
		       STASHER_NAMESPACE::nodeinfo::options_t::iterator>
		     iter(peernodeinfo.options
			  .equal_range(STASHER_NAMESPACE::nodeinfo::host_option));
	     iter.first != iter.second; ++iter.first)
	{
		try {
			LOG_INFO("Connecting to " << peername
				 << " on " << iter.first->second);

			return (x::httportmap::create(iter.first->second)
				->connect_any(peername, common.epollfd));
		} catch (const x::exception &e)
		{
			common.epollfd->epoll_wait(0);
			if (common.epollcb->timedout)
				throw;
			LOG_ERROR(iter.first->second << ": " << e);
		}
	}
	errno=ECONNREFUSED;
	throw SYSEXCEPTION("connect");
}

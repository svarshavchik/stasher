/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "clusterlistener.H"
#include "repomg.H"
#include "dirs.H"

#include <x/netaddr.H>
#include <x/dir.H>
#include <x/strftime.H>
#include <x/locale.H>
#include <x/epoll.H>
#include <x/ymd.H>
#include <x/ymdhms.H>
#include <x/weakptr.H>
#include <sys/time.h>

#include <iomanip>

LOG_CLASS_INIT(clusterlistenerObj);

x::property::value<x::ymd::interval> clusterlistenerObj
::certwarntime("certcheck::warntime", x::ymd::interval(0, 1, 0, 0));

x::property::value<x::ymd::interval> clusterlistenerObj
::certerrtime("certcheck::errtime", x::ymd::interval(0, 0, 1, 0));

clusterlistenerObj::clusterlistenerObj(const std::string &directoryArg)
	: directory(directoryArg),
	  portmapper(x::httportmap::create()),
	  networksocks(this),
	  privsock(init_sock(STASHER_NAMESPACE::privsockdir(directory), 0700,
			     STASHER_NAMESPACE::privsockname(directory))),
	  pubsock(init_sock(STASHER_NAMESPACE::pubsockdir(directory), 0755,
			    STASHER_NAMESPACE::pubsockname(directory)))
{
}

x::fd clusterlistenerObj::init_sock(const std::string &directory,
				    mode_t mode,
				    const std::string &dstname)

{
	x::dir::base::rmrf(directory);

	if (mkdir(directory.c_str(), mode) < 0)
		throw SYSEXCEPTION(directory);

	std::string tmpsockname(directory + "/socket.tmp");

	std::list<x::fd> dummy;

	x::netaddr::create(SOCK_STREAM, "file:" + tmpsockname)
		->bind(dummy, true);

	if (dummy.empty())
		throw EXCEPTION(tmpsockname + " creation failed");

	x::fd sock(dummy.front());

	sock->listen();
	sock->nonblock(true);

	chmod(tmpsockname.c_str(), 0777);
	if (rename(tmpsockname.c_str(), dstname.c_str()))
		throw SYSEXCEPTION("rename(" + tmpsockname + ","
				   + dstname + ")");
	return sock;
}

clusterlistenerObj::networksocks_t::networksocks_t(clusterlistenerObj *listener)

{
	listener->reload_internal();

	x::netaddr::create("", 0)->bind(*this, true);

	for (std::list<x::fd>::iterator b(begin()), e(end()); b != e; ++b)
	{
		(*b)->nonblock(true);
	}

	x::fd::base::listen(*this);

	if (!listener->portmapper->reg(listener->nodeName(), *this, true))
		throw EXCEPTION("Node already running");

	LOG_INFO("Registered " << listener->nodeName()
		 << " with the portmapper");
}

clusterlistenerObj::networksocks_t::~networksocks_t() noexcept
{
}

std::string clusterlistenerObj::getName() const
{
	return "listener:" + directory;
}

std::string clusterlistenerObj::nodeName() const noexcept
{
	try {
		return cert->get_dn(GNUTLS_OID_X520_COMMON_NAME);
	} catch (const x::exception &e)
	{
		LOG_FATAL(e);
		abort();
	}
}

std::string clusterlistenerObj::clusterName() const noexcept
{
	try {
		return cert->get_issuer_dn(GNUTLS_OID_X520_COMMON_NAME);
	} catch (const x::exception &e)
	{
		LOG_FATAL(e);
		abort();
	}
}

clusterlistenerObj::~clusterlistenerObj() noexcept
{
}

x::gnutls::credentials::certificate
clusterlistenerObj::credentials()
{
	std::lock_guard<std::mutex> lock(objmutex);

	return cred;
}

bool clusterlistenerObj::install(const std::string &certificate)
{
	{
		std::lock_guard<std::mutex> lock(objmutex);

		if (!repomg::update_certificate(directory, certificate))
			return false;
	}

	reload();
	return true;
}

void clusterlistenerObj::reload()
{
	try {
		reload_internal();
	} catch (const x::exception &e)
	{
		LOG_ERROR(e);
	}
}

void clusterlistenerObj::dispatch_checkcert()
{
	try {
		x::ymdhms now;

		time_t chk=({
				x::ymdhms t=now;

				static_cast<x::ymd &>(t)
					+= certwarntime.getValue();

				t;
			});


		time_t expiration_time=({
				std::lock_guard<std::mutex> lock(objmutex);

				cert->get_expiration_time();
			});

		if (expiration_time > chk)
			return;

		std::ostringstream msg;

		msg << "Certificate for " << nodeName()
		    << " expires on " <<
			x::strftime(x::tzfile::base::local(),
				    x::locale::create())
			(expiration_time)("%F");

		if ( expiration_time < ({
					x::ymdhms t=now;

					static_cast<x::ymd &>(t)
						+= certerrtime.getValue();

					t;
				}) )
		{
			LOG_ERROR(msg.str());
		}
		else
		{
			LOG_WARNING(msg.str());
		}
	} catch (const x::exception &e)
	{
		LOG_WARNING("Certificate expiration check: " << e);
	}
}

void clusterlistenerObj::reload_internal()
{
	std::list<x::gnutls::x509::crt> newcert, newclustcerts;

	x::gnutls::credentials::certificate
		newCert(repomg::init_credentials(directory, newcert,
						 newclustcerts));

	std::lock_guard<std::mutex> lock(objmutex);

	if (newcert.empty())
	{
		LOG_FATAL("Failed to reload node certificate");
		return;
	}

	if (!cert.null())
	{
		std::string newName=
			newcert.front()->get_dn(GNUTLS_OID_X520_COMMON_NAME);
		if (newName != nodeName())
		{
			LOG_FATAL("Updated node certificate's name has "
				  "changed from " + nodeName() + " to " +
				  newName);
			return;
		}
	}

	cred=newCert;
	cert=newcert.front();
	clustcerts=newclustcerts;
}

class clusterlistenerObj::epollcb : public x::epoll::base::callbackObj {

protected:
	x::weakptr<clusterlistenerptr> listener;
	void (clusterlistenerObj::*cb_func)(const x::fd &);

public:
	epollcb(const clusterlistenerptr &listenerArg,
		void (clusterlistenerObj::*cb_funcArg)(const x::fd &))
		: listener(listenerArg), cb_func(cb_funcArg)
	{
	}

	~epollcb() noexcept
	{
	}

protected:
	void event(const x::fd &fileDesc, event_t events)
	{
		clusterlistenerptr l(listener.getptr());

		if (!l.null())
			((*l).*cb_func)(fileDesc);
	}

public:
	static x::ref<epollcb> create(clusterlistenerObj *listener,
				      void (clusterlistenerObj::*cb_func)
				      (const x::fd &))

	{
		return x::ref<epollcb>::create(clusterlistenerptr(listener),
					       cb_func);
	}
};

void clusterlistenerObj::run(msgqueue_auto &msgqueue)
{
	x::httportmap portmapper_cpy=portmapper;

	portmapper=x::httportmapptr();

	x::epoll epollfd(x::epoll::create());

	epollfd->nonblock(true);

	msgqueue->getEventfd()
		->epoll_add(EPOLLIN, epollfd,
			    epollcb::create(this,
					    &clusterlistenerObj::tickle_eventfd));

	privsock->epoll_add(EPOLLIN, epollfd,
			    epollcb::create(this,
					    &clusterlistenerObj::accept_privsock));

	pubsock->epoll_add(EPOLLIN, epollfd,
			   epollcb::create(this,
					   &clusterlistenerObj::accept_pubsock));

	{
		auto cb=epollcb::create(this,
					&clusterlistenerObj::accept_network);

		epollfd->epoll_add(networksocks.begin(),
				   networksocks.end(),
				   EPOLLIN, cb);
	}

	try {
		while (1)
		{
			while (!msgqueue->empty())
				msgqueue.event();

			epollfd->epoll_wait();
		}
	} catch (const x::stopexception &e)
	{
	} catch (const x::exception &e)
	{
		LOG_FATAL(e);
	}
	LOG_INFO("Closing listening sockets on " << nodeName());

	for (std::list<x::fd>::iterator b=networksocks.begin(),
		     e=networksocks.end(); b != e; ++b)
		(*b)->close();

	portmapper_cpy->dereg(nodeName(), networksocks);
	LOG_INFO("Deregistered " << nodeName());
}

void clusterlistenerObj::accept_network(const x::fd &sock)
{
	try {
		x::sockaddrptr addr;

		x::fdptr fd(sock->accept(addr));

		if (!fd.null())
		{
			fd->keepalive(true);
			start_network(fd, addr);
		}
	} catch (const x::exception &e)
	{
		LOG_ERROR(e);
	}
}

void clusterlistenerObj::accept_privsock(const x::fd &sock)
{
	try {
		x::sockaddrptr addr;

		x::fdptr fd(sock->accept(addr));

		if (!fd.null())
			start_privsock(fd, addr);
	} catch (const x::exception &e)
	{
		LOG_ERROR(e);
	}
}

void clusterlistenerObj::accept_pubsock(const x::fd &sock)
{
	try {
		x::sockaddrptr addr;

		x::fdptr fd(sock->accept(addr));

		if (!fd.null())
			start_pubsock(fd, addr);
	} catch (const x::exception &e)
	{
		LOG_ERROR(e);
	}
}

void clusterlistenerObj::tickle_eventfd(const x::fd &sock)
{
	auto p=get_msgqueue();

	if (!p.null())
		p->getEventfd()->event();
}


std::string clusterlistenerObj::report(std::ostream &rep)
{
	{
		std::lock_guard<std::mutex> lock(objmutex);

		rep << "Node certificate: "
		    << cert->print(GNUTLS_X509_CRT_ONELINE)
		    << std::endl;

		for (std::list<x::gnutls::x509::crt>::const_iterator
			     b=clustcerts.begin(),
			     e=clustcerts.end(); b != e; ++b)
		{
			rep << "Cluster certificate: "
			    << (*b)->print(GNUTLS_X509_CRT_ONELINE)
			    << std::endl;
		}
	}

	for (std::list<x::fd>::iterator b=networksocks.begin(),
		     e=networksocks.end(); b != e; ++b)
	{
		rep << "Listening on " << (std::string)*(*b)->getsockname()
		    << std::endl;
	}

	return "listener(" + nodeName() + ")";
}

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "stoppablethreadtracker.H"
#include "trandistributor.H"
#include <x/options.H>

static bool server_connect_terminate_hook,
	client_connect_terminate_hook,

	server_connected_read_timeout,
	server_connected_write_timeout,

	client_connected_read_timeout,
	client_connected_write_timeout;



#define CLUSTERCONNECTED_CONNECT_SERVER_INIT_HOOK(arg)	\
	if (server_connect_terminate_hook) stop()

#define CLUSTERCONNECTED_CONNECT_CLIENT_INIT_HOOK(arg)	\
	if (client_connect_terminate_hook) stop()

#include "clusterconnecter.C"
#include "threadmgr.H"

#include <iostream>
#include <sstream>
#include <list>
#include <x/dir.H>

class serverclientcommon {

public:

	x::fdtimeoutptr timeout;
	x::fd::base::inputiter iter, end_iter;
	x::fd::base::outputiter out_iter;

	void init(const x::fd &socket,
		  const x::gnutls::sessionptr &session,
		  const x::fd::base::inputiter &inputiter,
		  const x::eventfd &terminate_eventfd)
	{
		timeout=x::fdtimeout::create(socket);

		if (session.null())
		{
			iter=x::fd::base::inputiter(timeout);
			out_iter=x::fd::base::outputiter(timeout);
		}
		else
		{
			session->setTransport(timeout);
			iter=inputiter;
			out_iter=x::fd::base::outputiter(session,
						   session->get_max_size());
		}
		timeout->set_terminate_fd(terminate_eventfd);

	}
};

class myserver : public clusterconnecterObj {

	LOG_CLASS_SCOPE;
public:
	myserver();
	~myserver() noexcept;

	void connected(const std::string &peername,
		       const x::fd &socket,
		       const x::gnutls::sessionptr &session,
		       const x::fd::base::inputiter &inputiter,
		       const nodeclusterstatus &peerstatus,
		       time_t timestamp,
		       const x::uuid &connuuid,
		       const clusterlistener &listener);
};

LOG_CLASS_INIT(myserver);

class mydistributorObj : public trandistributorObj {

public:
	mydistributorObj() throw(x::exception) {}
	~mydistributorObj() throw() {}

	void internal_transaction(const x::ref<internalTransactionObj>
				  &tran) {}
};

myserver::myserver()
	: clusterconnecterObj("Server thread")
{
}

myserver::~myserver() noexcept
{
}

void myserver::connected(const std::string &peername,
			 const x::fd &socket,
			 const x::gnutls::sessionptr &session,
			 const x::fd::base::inputiter &inputiter,
			 const nodeclusterstatus &peerstatus,
			 time_t timestamp,
			 const x::uuid &connuuid,
			 const clusterlistener &listener)

{
	serverclientcommon c;

	c.init(socket, session, inputiter, terminate_eventfd);

	x::deserialize::iterator<x::fd::base::inputiter>
		deser(c.iter, c.end_iter);

	x::serialize::iterator<x::fd::base::outputiter>
		ser(c.out_iter);

	try {
		if (server_connected_read_timeout)
		{
			server_connected_read_timeout=false;
			stop();
		}

		while (c.iter != c.end_iter)
		{
			std::string s;

			deser(s);
			if (server_connected_write_timeout)
			{
				server_connected_write_timeout=false;
				stop();
			}
			ser(s);
			c.out_iter.flush();
		}
		if (!session.null())
		{
			int dummy;

			if (!session->bye(dummy))
				throw EXCEPTION("TLS bye failed (server)");
		}
	} catch (const x::exception &e)
	{
		LOG_ERROR(e);
	}
}

class myclient : public clusterconnecterObj {

	LOG_CLASS_SCOPE;
public:
	myclient();
	~myclient() noexcept;

	std::list<std::string> strings;

	bool usedtls;
	bool success;

	void connected(const std::string &peername,
		       const x::fd &socket,
		       const x::gnutls::sessionptr &session,
		       const x::fd::base::inputiter &inputiter,
		       const nodeclusterstatus &peerstatus,
		       time_t timestamp,
		       const x::uuid &connuuid,
		       const clusterlistener &listener);
};

LOG_CLASS_INIT(myclient);

myclient::myclient()
	: clusterconnecterObj("Client thread"), success(false)
{
}

myclient::~myclient() noexcept
{
}

void myclient::connected(const std::string &peername,
			 const x::fd &socket,
			 const x::gnutls::sessionptr &session,
			 const x::fd::base::inputiter &inputiter,
			 const nodeclusterstatus &peerstatus,
			 time_t timestamp,
			 const x::uuid &connuuid,
			 const clusterlistener &listener)
{
	serverclientcommon c;

	c.init(socket, session, inputiter, terminate_eventfd);

	usedtls = !session.null();
	success=false;

	x::deserialize::iterator<x::fd::base::inputiter>
		deser(c.iter, c.end_iter);

	x::serialize::iterator<x::fd::base::outputiter>
		ser(c.out_iter);

	try {
		for (std::list<std::string>::iterator b(strings.begin()),
			     e(strings.end()); b != e; ++b)
		{
			if (client_connected_write_timeout)
			{
				client_connected_write_timeout=false;
				stop();
			}
			ser(*b);
			c.out_iter.flush();

			if (client_connected_read_timeout)
			{
				client_connected_read_timeout=false;
				stop();
			}
			std::string s;

			deser(s);

			if (s != *b)
			{
				std::ostringstream o;

				o << "Client round-trip failed for string of "
					"length " << b->size();

				throw EXCEPTION(o.str());
			}
		}
		if (!session.null())
		{
			int dummy;

			if (!session->bye(dummy))
				throw EXCEPTION("TLS bye failed (client)");
		}
		success=true;
	} catch (const x::exception &e)
	{
		LOG_ERROR(e);
	}
}

class myserver_owner {

public:
	x::ref<myserver> instance;

	x::ref<threadmgrObj<x::ref<myserver> > > thread;

	myserver_owner()
		: instance(x::ref<myserver>::create()),
		  thread(x::ref<threadmgrObj<x::ref<myserver> > >::create())
	{
	}

	void start(const clusterinfo &cluster,
		   const clusterlistener &listener,
		   const x::fd &fd,
		   const tobjrepo &repo,
		   const x::ptr<trandistributorObj> &distributor)
	{
		thread->run(instance,
			    cluster, listener, fd, repo, distributor);
	}

};

class mylistener : public clusterlistenerObj {

public:
	myserver_owner server;

	clusterinfo info;
	std::string directory;

	mylistener(const std::string &directoryArg,
		   const clusterinfo &infoArg);
	~mylistener() noexcept;

	void start_network(const x::fd &sock,
			   const x::sockaddr &addr);

	//! Handle a private socket connection
	void start_privsock(const x::fd &sock,
			    const x::sockaddr &addr);

	//! Handle a public socket connection
	void start_pubsock(const x::fd &sock,
			   const x::sockaddr &addr);

	void run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
		 start_thread_sync &sync_arg)
	{
		msgqueue_auto msgqueue(this);

		threadmsgdispatcher_mcguffin=x::ptr<x::obj>();
		sync_arg->thread_started();
		clusterlistenerObj::run(msgqueue);
	}

	void foo() override {}
};

class listener_owner {

public:
	x::ref<mylistener> listener;
	x::ref<threadmgrObj<x::ref<mylistener> > > thread;

	listener_owner(const std::string &directoryArg,
		       const clusterinfo &infoArg)
		: listener(x::ref<mylistener>::create(directoryArg,
						      infoArg)),
		  thread(x::ref<threadmgrObj<x::ref<mylistener> > >::create())
	{
		thread->start_thread(listener);
	}
};


mylistener::mylistener(const std::string &directoryArg,
		       const clusterinfo &infoArg)
	: clusterlistenerObj(directoryArg),
	  info(infoArg), directory(directoryArg)
{
}

mylistener::~mylistener() noexcept
{
}

void mylistener::start_network(const x::fd &sock,
			       const x::sockaddr &addr)

{
	server.start(info, clusterlistener(this), sock,
		     tobjrepo::create(directory),
		     x::ptr<mydistributorObj>::create());
}

void mylistener::start_privsock(const x::fd &sock,
				const x::sockaddr &addr)

{
}

void mylistener::start_pubsock(const x::fd &sock,
			       const x::sockaddr &addr)

{
}

static void test1(const std::string &nodeadir,
		  const std::string &nodebdir,
		  bool useencryption)
{
	STASHER_NAMESPACE::nodeinfo nodeainfo, nodebinfo;

	nodeainfo.options.insert(std::make_pair(std::string("host"),
						std::string("localhost")));
	nodeainfo.useencryption(useencryption);
	nodebinfo.options=nodeainfo.options;

	clusterinfoptr infoa, infob;

	{
		STASHER_NAMESPACE::nodeinfomap info;

		info[tstnodes::getnodefullname(0)]=nodeainfo;
		info[tstnodes::getnodefullname(1)]=nodebinfo;

		infoa=clusterinfo::create(tstnodes::getnodefullname(0),
					  "test",
					  STASHER_NAMESPACE::stoppableThreadTrackerImpl::create()
					  ->getTracker(), info);

		infob=clusterinfo::create(tstnodes::getnodefullname(1),
					  "test",
					  STASHER_NAMESPACE::stoppableThreadTrackerImpl::create()
					  ->getTracker(), info);
	}

	listener_owner listenera(nodeadir, infoa),
		listenerb(nodebdir, infob);


	auto client=x::ref<myclient>::create();
	auto client_owner=x::ref<threadmgrObj<decltype(client)> >::create();

	{
		std::list<std::string> &l=client->strings;

		l.push_back(std::string(100, 'A'));
		l.push_back(std::string(100000, 'B'));
	}

	client_owner->run(client,
			  infob,
			  listenerb.listener,
			  tstnodes::getnodefullname(0),
			  tobjrepo::create(nodebdir),
			  x::ptr<mydistributorObj>::create());
	client_owner->wait();

	listenera.listener->server.thread->wait();

	if (!client->success)
		throw EXCEPTION("Success flag was not set");

}

static void iter_timeout(const std::string &nodeadir,
			 const std::string &nodebdir,
			 bool useencryption,

			 bool server_read,
			 bool server_write,

			 bool client_read,
			 bool client_write,
			 const char *what)
{
	std::cout << what << std::endl;

	server_connected_read_timeout=server_read;
	server_connected_write_timeout=server_write;

	client_connected_read_timeout=client_read;
	client_connected_write_timeout=client_write;

	std::cerr << "The following errors are expected:" << std::endl;
	try {
		test1(nodeadir, nodebdir, useencryption);
	} catch (const x::exception &e)
	{
		std::cerr << "Caught: " << e << std::endl;
	}

	if (server_connected_read_timeout ||
	    server_connected_write_timeout ||
	    client_connected_read_timeout ||
	    client_connected_write_timeout)
		throw EXCEPTION("Armed trigger did not fire");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	tstnodes nodes(2);

	try {

		ALARM(120);

		std::cout << "plain" << std::endl;
		test1(tstnodes::getnodedir(0), tstnodes::getnodedir(1), false);
		std::cout << "encrypted" << std::endl;
		test1(tstnodes::getnodedir(0), tstnodes::getnodedir(1), true);

		std::cout << "Testing client timeout. The following errors are expected:"
			  << std::endl;

		try {
			client_connect_terminate_hook=true;
			test1(tstnodes::getnodedir(0), tstnodes::getnodedir(1), false);
		} catch (const x::exception &e)
		{
			std::cerr << "Caught: " << e << std::endl;
		}

		std::cout << "Testing server timeout. The following errors are expected:"
			  << std::endl;

		client_connect_terminate_hook=false;
		try {
			server_connect_terminate_hook=true;
			test1(tstnodes::getnodedir(0), tstnodes::getnodedir(1), false);
		} catch (const x::exception &e)
		{
			std::cerr << "Caught: " << e << std::endl;
		}
		server_connect_terminate_hook=false;

		iter_timeout(tstnodes::getnodedir(0), tstnodes::getnodedir(1), false,
			     true, false, false, false,
			     "iter timeout: plain, server, read");

		iter_timeout(tstnodes::getnodedir(0), tstnodes::getnodedir(1), true,
			     true, false, false, false,
			     "iter timeout: tls, server, read");

		iter_timeout(tstnodes::getnodedir(0), tstnodes::getnodedir(1), false,
			     false, true, false, false,
			     "iter timeout: plain, server, write");

		iter_timeout(tstnodes::getnodedir(0), tstnodes::getnodedir(1), true,
			     false, true, false, false,
			     "iter timeout: tls, server, write");

		iter_timeout(tstnodes::getnodedir(0), tstnodes::getnodedir(1), false,
			     false, false,true, false,
			     "iter timeout: plain, client, read");

		iter_timeout(tstnodes::getnodedir(0), tstnodes::getnodedir(1), true,
			     false, false, true, false,
			     "iter timeout: tls, client, read");

		iter_timeout(tstnodes::getnodedir(0), tstnodes::getnodedir(1), false,
			     false, false, false, true,
			     "iter timeout: plain, client, write");

		iter_timeout(tstnodes::getnodedir(0), tstnodes::getnodedir(1), true,
			     false, false, false, true,
			     "iter timeout: tls, client, write");

	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "stasher/client.H"
#include "stasher/userinit.H"
#include "stasher/process_request.H"
#include <x/options.H>
#include <x/fditer.H>
#include <x/deserialize.H>
#include <x/destroycallbackflag.H>

static std::mutex test1_mutex;
static std::condition_variable test1_cond;
static bool test1_enabled=false;
static size_t client_queued_cnt;
static size_t client_sent_cnt;
static size_t server_queue_cnt;


#define DEBUG_TEST_GETQUEUE_CLIENTIMPL_RECEIVED() do {		\
		std::unique_lock<std::mutex> lock(test1_mutex);		\
		if (test1_enabled)				\
		{						\
			client_queued_cnt=getqueue->size();	\
			client_sent_cnt=getpending_cnt;		\
			test1_cond.notify_all();			\
		}						\
	} while (0)

#define DEBUG_TEST_GETQUEUE_SERVER_RECEIVED() do {		\
		std::unique_lock<std::mutex> lock(test1_mutex);		\
		if (test1_enabled)				\
		{						\
			server_queue_cnt=getqueue_cnt;		\
			test1_cond.notify_all();			\
		}						\
	} while (0)

#include "localconnection.C"
#include "client.C"

static void test1(tstnodes &t)
{
	x::property::load_property("connection::maxgetobjects", "10",
				   true, true);

	std::cerr << "test 1" << std::endl;

	{
		std::vector<tstnodes::noderef> tnodes;

		t.init(tnodes);
		t.startmastercontrolleron0_int(tnodes);
	}

	{
		std::unique_lock<std::mutex> lock(test1_mutex);
		test1_enabled=true;
		client_queued_cnt=0;
		client_sent_cnt=0;
		server_queue_cnt=0;
	}

	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	tnodes[0]->start(true);
	tnodes[1]->start(true);
	tnodes[0]->debugWaitFullQuorumStatus(false);
	tnodes[1]->debugWaitFullQuorumStatus(false);

	std::cerr << "started" << std::endl;

	STASHER_NAMESPACE::client cl=
		STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	std::cerr << "connected" << std::endl;

	const size_t n_reqs=40;

	STASHER_NAMESPACE::client::base::getreqptr contents[n_reqs];
	STASHER_NAMESPACE::client::base::requestptr reqs[n_reqs];

	for (size_t i=0; i<n_reqs; ++i)
	{
		contents[i]=STASHER_NAMESPACE::client::base::getreq::create();
		contents[i]->objects.insert("x");
		reqs[i]=cl->get_request(contents[i]).second;
	}

	std::cerr << "requests sent" << std::endl;

	{
		std::unique_lock<std::mutex> lock(test1_mutex);

		while ((std::cerr << client_queued_cnt << " queued, "
			<< client_sent_cnt << " sent, "
			<< server_queue_cnt << " server queue size"
			<< std::endl),
		       client_queued_cnt != n_reqs - 10 ||
		       client_sent_cnt != 10 ||
		       server_queue_cnt != 10)
		{
			test1_cond.wait(lock);
		}
	}
	std::cerr << "Establishing quorum" << std::endl;

	tnodes[0]->listener->connectpeers();

	for (size_t i=0; i<n_reqs; ++i)
		reqs[i]->wait();

	{
		std::unique_lock<std::mutex> lock(test1_mutex);
		test1_enabled=false;
	}
}

static void test2(tstnodes &t)
{
	std::cerr << "test 2" << std::endl;

	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	tnodes[0]->start(true);
	tnodes[1]->start(true);
	tnodes[0]->debugWaitFullQuorumStatus(false);
	tnodes[1]->debugWaitFullQuorumStatus(false);

	std::cerr << "started" << std::endl;

	STASHER_NAMESPACE::client cl=
		STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	cl->debugSetLimits(STASHER_NAMESPACE::userinit(100, 100,
						       8192, 8192, 10));

	const size_t n_reqs=40;

	std::cerr << "The following error is expected:" << std::endl;
	STASHER_NAMESPACE::client::base::getreqptr contents[n_reqs];
	STASHER_NAMESPACE::client::base::requestptr reqs[n_reqs];

	for (size_t i=0; i<n_reqs; ++i)
	{
		contents[i]=STASHER_NAMESPACE::client::base::getreq::create();
		contents[i]->objects.insert("x");
		reqs[i]=cl->get_request(contents[i]).second;
	}

	cl->debugWaitDisconnection();
}

static void test3(tstnodes &t)
{
	std::cerr << "test 3" << std::endl;

	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	std::cerr << "started" << std::endl;

	STASHER_NAMESPACE::client cl=
		STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	for (size_t i=0; i<10; ++i)
	{
		auto tran=STASHER_NAMESPACE::client::base
			::transaction::create();
		std::ostringstream o;
		o << i;
		std::string s=o.str();

		tran->newobj(s, s + "\n");
		auto res=cl->put(tran);

		if (res->status != STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION(x::tostring(res->status));
	}

	auto wl=x::weaklist<x::obj>::create();

	for (size_t i=0; i<10; ++i)
	{
		x::ref<x::obj> mcguffin=x::ref<x::obj>::create();

		wl->push_back(mcguffin);

		std::ostringstream o;
		o << i;
		std::string s=o.str();

		auto req=STASHER_NAMESPACE::client::base::getreq::create();

		req->openobjects=true;
		req->objects.insert(s);

		STASHER_NAMESPACE::process_request
			(cl->get_request(req),
			 [s, mcguffin, cl]
			 (const STASHER_NAMESPACE ::getresults &res)
			 {
				 auto contents=res->objects;

				 std::cout << s << ": get: "
					   << contents->errmsg << std::endl;

				 auto tran=STASHER_NAMESPACE::client::base
					 ::transaction::create();

				 auto fd=(*contents)[s].fd;

				 std::string orig=
					 std::string(x::fdinputiter(fd),
						     x::fdinputiter());

				 tran->updobj(s, (*contents)[s].uuid,
					      orig + orig);

				 STASHER_NAMESPACE::process_request
					 (cl->put_request(tran),
					  [s, mcguffin]
					  (const STASHER_NAMESPACE::putresults
					   &res)
					  {
						  std::cout << s << ": put: "
							    << x::tostring
							  (res->status)
							    << std::endl;
					  });
			 });
	}

	std::cout << "Waiting" << std::endl;
	bool keepgoing;

	do
	{
		keepgoing=false;

		for (auto &mcguffin: *wl)
		{
			auto ptr=mcguffin.getptr();

			if (ptr.null())
				continue;
			keepgoing=true;
			x::destroyCallbackFlag::base::guard guard;

			guard(x::ref<x::obj>(ptr));

			ptr=decltype(ptr)();
			break;
		}
	} while (keepgoing);

	for (size_t i=0; i<10; ++i)
	{
		x::ref<x::obj> mcguffin=x::ref<x::obj>::create();

		wl->push_back(mcguffin);

		std::ostringstream o;
		o << i;
		std::string s=o.str();

		auto req=STASHER_NAMESPACE::client::base::getreq::create();

		req->openobjects=true;
		req->objects.insert(s);

		auto objects=cl->get(req)->objects;

		auto fd=(*objects)[s].fd;

		std::string orig=std::string(x::fdinputiter(fd),
					     x::fdinputiter());

		if (orig != s + "\n" + s + "\n")
			throw EXCEPTION("Object " + s + " is something else");
	}
}


int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		tstnodes nodes(2);

		test1(nodes);
		test2(nodes);
		test3(nodes);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

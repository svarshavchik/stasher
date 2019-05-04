/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "stasher/client.H"
#include <x/options.H>

class sendstop : virtual public x::obj {

	LOG_CLASS_SCOPE;

public:
	sendstop() {}
	~sendstop() {}

	void run(const STASHER_NAMESPACE::client &arg)
	{
		try {
			arg->shutdown();
		} catch (const x::exception &e)
		{
			LOG_ERROR(e);
		}
	}
};

LOG_CLASS_INIT(sendstop);

static void test1(tstnodes &t)
{
	node test(tstnodes::getnodedir(0));

	test.start(false);
	test.debugWaitFullQuorumStatus(true);

	STASHER_NAMESPACE::client cl=
		STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	STASHER_NAMESPACE::client::base::transaction tran=
		STASHER_NAMESPACE::client::base::transaction::create();

	tran->newobj("obj1", "obj1_value\n");
	STASHER_NAMESPACE::putresults res=cl->put(tran);

	if (res->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION(x::to_string(res->status));

	x::ref<sendstop> thr(x::ref<sendstop>::create());

	auto runthr=x::run(thr, STASHER_NAMESPACE::client::base
			   ::admin(tstnodes::getnodedir(0)));

	test.wait();
	runthr->wait();
	test=node();

	bool error=false;

	STASHER_NAMESPACE::client::base::getreq req=
		STASHER_NAMESPACE::client::base::getreq::create();

	req->objects.insert("obj1");

	try {
		auto resp=cl->get(req);

		if (!resp->objects->succeeded)
		{
			error=true;
			std::cerr << "Error: " << resp->objects->errmsg
				  << std::endl;
		}

	} catch(...) {
		error=true;
	}

	if (!error)
		throw EXCEPTION("How did I get a response from a downed node?");

	for (size_t i=0; i<5; ++i)
	{
		auto dir_resp=cl->getdir("");

		if (dir_resp->status !=
		    STASHER_NAMESPACE::req_processed_stat)
			std::cerr << "DIR error: "
				  << x::to_string(dir_resp->status)
				  << std::endl;

		req=STASHER_NAMESPACE::client::base::getreq::create();
		req->objects.insert("obj1");

		auto get_resp=cl->get(req);

		if (get_resp->objects->succeeded)
			return;

		std::cerr << "GET error: " << get_resp->objects->errmsg
			  << std::endl;

		if (i == 0)
		{
			test=node(tstnodes::getnodedir(0));
			std::cerr << "Starting" << std::endl;
			test.start(false);
			test.debugWaitFullQuorumStatus(true);
			std::cerr << "Started" << std::endl;
		}
	}

	throw EXCEPTION("Could not reconnect to the node");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		x::dir::base::rmrf("conftestdir.tst");
		std::cout << "test1" << std::endl;
		tstnodes nodes(1);
		test1(nodes);
		x::dir::base::rmrf("conftestdir.tst");
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "stasher/client.H"

#include <x/options.H>

class sendstop : virtual public x::obj {

	LOG_CLASS_SCOPE;

public:
	sendstop() {}
	~sendstop() noexcept {}

	void run(const STASHER_NAMESPACE::client &start_arg)
	{
		try {
			start_arg->shutdown();
		} catch (const x::exception &e)
		{
			LOG_ERROR(e);
		}
	}
};

LOG_CLASS_INIT(sendstop);

static void test1(tstnodes &t)
{
	node test(tstnodes::getnodedir(0)), test2(tstnodes::getnodedir(1));

        {
                STASHER_NAMESPACE::nodeinfomap clusterinfo;

                STASHER_NAMESPACE::nodeinfo info;

                info.options.insert(std::make_pair(STASHER_NAMESPACE::nodeinfo::host_option,
                                                   "localhost"));


		for (size_t i=0; i<2; ++i)
		{
			clusterinfo[tstnodes::getnodefullname(i)]=info;
		}

		repoclusterinfoObj::saveclusterinfo(test.repo, clusterinfo);
		repoclusterinfoObj::saveclusterinfo(test2.repo, clusterinfo);
        }

	test.start(false);
	test2.start(false);

	test.debugWaitFullQuorumStatus(true);
	test2.debugWaitFullQuorumStatus(true);

	STASHER_NAMESPACE::client
		cl=STASHER_NAMESPACE::client::base::admin(tstnodes::getnodedir(0));

	std::cerr << cl->getserverstatus()->report << std::flush;

	{
		STASHER_NAMESPACE::client cl2=
			STASHER_NAMESPACE::client::base::admin(tstnodes::getnodedir(1));

		std::cerr << cl2->getserverstatus()->report << std::flush;
	}

	auto runthr=x::run(x::ref<sendstop>::create(), cl);
	test.wait();
	runthr->wait();

	auto dir_resp=cl->getdir("");

	if (dir_resp->status == STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION("Still connected to the server, when we should be disconnected");
}

static void test2(tstnodes &t)
{
	try {
		STASHER_NAMESPACE::client::base::admin(tstnodes::getnodedir(0));
	} catch (const x::exception &e)
	{
		std::cerr << "Expected exception: " << e << std::endl;
		return;
	}

	throw EXCEPTION("Somehow connected to a nonexistent server");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	x::property::load_property("reconnect", "4", true, true);

	try {
		tstnodes nodes(2);

		std::cout << "test1" << std::endl;
		test1(nodes);

		std::cout << "test2" << std::endl;
		test2(nodes);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

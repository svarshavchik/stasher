/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "stasher/client.H"
#include "stasher/process_request.H"
#include "clusterlistenerimpl.H"
#include <fstream>

static void test1(tstnodes &t, const char *argv0)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	clusterlistenerimplObj::appsdir.setValue("appsdir.tmp");
	mkdir("appsdir.tmp", 0777);
	
	{
		std::ofstream o("appsdir.tmp/appsdir.tmp");

		o << "PATH \"" << argv0 << "\"" << std::endl;
		o << "ROOT sandbox/foo" << std::endl;
		o.close();
	}

	auto client=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	auto tran=STASHER_NAMESPACE::client::base::transaction::create();

	tran->newobj("bar", "foobar\n");
	auto res=client->put(tran);

	if (res->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION("put: " + x::tostring(res->status));

	client=STASHER_NAMESPACE::client::base::admin(tstnodes::getnodedir(0));

	{
		STASHER_NAMESPACE::contentsptr res;
		std::mutex mutex;
		std::condition_variable cond;
		bool received=false;

		auto req=STASHER_NAMESPACE::client::base::getreq::create();
		req->objects.insert("sandbox/foo/bar");
		req->openobjects=true;

		STASHER_NAMESPACE
			::process_request(client->get_request(req),
					  [&res, &mutex, &cond, &received]
					  (const STASHER_NAMESPACE::
					   getresults &resArg)
					  {
						  std::unique_lock<std::mutex>
							  l(mutex);

						  res=resArg->objects;
						  received=true;
						  cond.notify_all();
					  });

		{
			std::unique_lock<std::mutex> l(mutex);

			cond.wait(l, [&received] { return received; });
		}

		if (!res->succeeded)
			throw EXCEPTION(res->errmsg);

		auto p=res->find("sandbox/foo/bar");

		std::string line;

		if (p == res->end()
		    || (std::getline(*p->second.fd->getistream(), line),
			line) != "foobar")
		{
			throw EXCEPTION("appsdir test failed");
		}
	}
}

int main(int argc, char **argv)
{
	int rc=0;

#include "opts.parse.inc.tst.C"

	try {
		x::dir::base::rmrf("appsdir.tmp");
		tstnodes nodes(2);

		test1(nodes, argv[0]);
		x::dir::base::rmrf("appsdir.tmp");
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		rc=1;
	}

	return rc;
}

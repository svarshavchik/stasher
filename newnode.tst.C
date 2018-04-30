/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "stasher/client.H"
#include "repomg.H"
#include <x/options.H>
#include <x/serialize.H>
#include <iterator>

class test1_cb : public STASHER_NAMESPACE::serverstatuscallbackObj {

public:
	std::mutex mutex;
	std::condition_variable cond;
	std::string dir;

	bool received;
	STASHER_NAMESPACE::clusterstate status;

	test1_cb(const std::string &dirArg) : dir(dirArg), received(false) {}
	~test1_cb() {}

	void state(const STASHER_NAMESPACE::clusterstate &inquorum)
		override
	{
		std::lock_guard<std::mutex> lock(mutex);

		received=true;
		status=inquorum;
		std::cout << dir << ": " << status.nodes.size()
			  << " peers: "
			  << x::tostring(status) << std::endl;
		cond.notify_all();
	}

	void serverinfo(const STASHER_NAMESPACE::userhelo &serverinfo)
		override
	{
	}
};

void test1(tstnodes &t)
{
	std::cout << "test1" << std::endl;

	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	tnodes[0]->start(false);
	tnodes[1]->start(false);

	STASHER_NAMESPACE::client
		cl0=STASHER_NAMESPACE::client::base::admin(tstnodes::getnodedir(0)),
		cl1=STASHER_NAMESPACE::client::base::admin(tstnodes::getnodedir(1));


	auto cb0=x::ref<test1_cb>::create(tstnodes::getnodedir(0)),
		cb1=x::ref<test1_cb>::create(tstnodes::getnodedir(1));

	auto sub0=cl0->subscribeserverstatus(cb0);
	auto sub1=cl1->subscribeserverstatus(cb1);

	{
		std::unique_lock<std::mutex> lock(cb0->mutex);

		while (!cb0->received)
			cb0->cond.wait(lock);
	}
	{
		std::unique_lock<std::mutex> lock(cb1->mutex);

		while (!cb1->received)
			cb1->cond.wait(lock);
	}
	std::cout << "Two nodes are ready" << std::endl;

	STASHER_NAMESPACE::nodeinfomap clusterinfo;

	STASHER_NAMESPACE::nodeinfo info;

	info.options.insert(std::make_pair(STASHER_NAMESPACE::nodeinfo::host_option,
					   "localhost"));

	for (size_t i=0; i<tnodes.size(); ++i)
	{
		clusterinfo[t.getnodefullname(i)]=info;
	}

	STASHER_NAMESPACE::client::base::transaction
		tran=STASHER_NAMESPACE::client::base::transaction::create();

	std::string s;

	typedef std::back_insert_iterator<std::string> ins_iter_t;

	ins_iter_t ins_iter(s);

	x::serialize::iterator<ins_iter_t> ser_iter(ins_iter);

	ser_iter(clusterinfo);

	tran->newobj(STASHER_NAMESPACE::client::base::clusterconfigobj, s);

	STASHER_NAMESPACE::putresults results=cl0->put(tran);

	if (results->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION(x::tostring(results->status));

	{
		std::unique_lock<std::mutex> lock(cb1->mutex);

		while (cb1->status.nodes.size() != 1 ||
		       !cb1->status.full)
			cb1->cond.wait(lock);
	}
	std::cout << "Done" << std::endl;
}

int main(int argc, char **argv)
{
#define ENABLE_NEWNODERECONNECT

#include "opts.parse.inc.tst.C"

	x::property::load_property("reconnect", "4", true, true);

	try {
		{
			tstnodes nodes(2);

			test1(nodes);
		}

	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

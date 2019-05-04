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

class test1_cb : public STASHER_NAMESPACE::serverstatuscallbackObj {

public:
	std::mutex mutex;
	std::condition_variable cond;

	bool received;
	bool info_received;
	STASHER_NAMESPACE::clusterstate status;
	STASHER_NAMESPACE::userhelo curserverinfo;

	test1_cb() : received(false), info_received(false) {}
	~test1_cb() {}

	void state(const STASHER_NAMESPACE::clusterstate &inquorum)
		override
	{
		std::unique_lock<std::mutex> lock(mutex);

		received=true;
		status=inquorum;
		cond.notify_all();
	}

	void serverinfo(const STASHER_NAMESPACE::userhelo &serverinfoArg)
		override
	{
		std::unique_lock<std::mutex> lock(mutex);

		curserverinfo=serverinfoArg;
		info_received=true;
		cond.notify_all();
	}

};

void setclusterinfo(std::vector<tstnodes::noderef> &tnodes,
		    tstnodes &t)
{
	STASHER_NAMESPACE::nodeinfomap clusterinfo;

	STASHER_NAMESPACE::nodeinfo info;

	info.options.insert(std::make_pair(STASHER_NAMESPACE::nodeinfo::host_option,
					   "localhost"));

	for (size_t i=0; i<tnodes.size(); ++i)
	{
		clusterinfo[t.getnodefullname(i)]=info;
	}

	for (size_t i=0; i<tnodes.size(); ++i)
	{
		repoclusterinfoObj::saveclusterinfo(tnodes[i]->repo,
						    clusterinfo);
	}
}

void test1(tstnodes &t)
{
	std::cout << "test1" << std::endl;

	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	setclusterinfo(tnodes, t);

	tnodes[0]->start(false);

	std::cout << "started node 0" << std::endl;
	STASHER_NAMESPACE::client
		cl0=STASHER_NAMESPACE::client::base::admin(tstnodes::getnodedir(0));

	std::cout << "connected" << std::endl;
	auto cb=x::ref<test1_cb>::create();

	auto sub0=cl0->subscribeserverstatus(cb);

	std::cout << "subscribed" << std::endl;
	{
		std::unique_lock<std::mutex> lock(cb->mutex);

		cb->cond.wait(lock, [&cb] {
				return cb->received && cb->info_received;
			});

		if (cb->status.majority)
			throw EXCEPTION("Unexpected initial quorum status:\n"
					+ x::to_string(cb->status));

		std::cout << "Connected to "
			  << cb->curserverinfo.nodename
			  << std::endl;
	}
	std::cout << "notified, no quorum" << std::endl;

	tnodes[1]->start(false);
	{
		std::unique_lock<std::mutex> lock(cb->mutex);

		while (!cb->status.majority)
			cb->cond.wait(lock);

		if (cb->status.full)
			throw EXCEPTION("How can I have a full quorum?\n" +
					x::to_string(cb->status));
	}
	std::cout << "notified, majority quorum" << std::endl;

	tnodes[2]->start(false);
	{
		std::unique_lock<std::mutex> lock(cb->mutex);

		while (!cb->status.full)
			cb->cond.wait(lock);
	}
	std::cout << "notified, full quorum" << std::endl;
}

void test2(tstnodes &t, int which1st)
{
	std::cout << "test2 (" << which1st << ")" << std::endl;

	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	setclusterinfo(tnodes, t);

	tnodes[which1st]->start(false);

	std::cout << "started node " << which1st << std::endl;
	STASHER_NAMESPACE::client
		cl0=STASHER_NAMESPACE::client::base::admin(tstnodes::getnodedir(which1st));

	std::cout << "connected" << std::endl;

	auto cb=x::ref<test1_cb>::create();
	auto sub0=cl0->subscribeserverstatus(cb);

	std::cout << "subscribed" << std::endl;
	{
		std::unique_lock<std::mutex> lock(cb->mutex);

		while (!cb->received)
			cb->cond.wait(lock);
		if (cb->status.majority)
			throw EXCEPTION("Unexpected initial quorum status:\n"
					+ x::to_string(cb->status));
	}
	std::cout << "notified, no quorum" << std::endl;

	if (clusterinfoObj::status(tnodes[which1st]->repocluster)->master
	    != tnodes[which1st]->repocluster->getthisnodename())
	{
		throw EXCEPTION("[OLDESTWINS] failed (1)");
	}

	sleep(2);
	tnodes[1-which1st]->repocluster->debugUpdateTimestamp();

	tnodes[1-which1st]->start(false);
	{
		std::unique_lock<std::mutex> lock(cb->mutex);

		while (!cb->status.full)
			cb->cond.wait(lock);
	}
	std::cout << "notified, full quorum" << std::endl;

	if (clusterinfoObj::status(tnodes[which1st]->repocluster)->master
	    != tnodes[which1st]->repocluster->getthisnodename())
	{
		throw EXCEPTION("[OLDESTWINS] failed (2)");
	}

	if (clusterinfoObj::status(tnodes[1 - which1st]->repocluster)->master
	    != tnodes[which1st]->repocluster->getthisnodename())
	{
		throw EXCEPTION("[OLDESTWINS] failed (3)");
	}
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	x::property::load_property("reconnect", "4", true, true);

	try {
		{
			tstnodes nodes(3);

			test1(nodes);
		}

		{
			tstnodes nodes(2);

			test2(nodes, 0);
			test2(nodes, 1);
		}
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

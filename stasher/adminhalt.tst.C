/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "repocontrollermaster.H"
#include "repopeerconnection.H"
#include "stasher/client.H"
#include <x/options.H>

static void test1(tstnodes &t)
{
	std::cerr << "test1" << std::endl;

	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	repocontrollermasterptr master=t.startmastercontrolleron0(tnodes);

	tnodes[0]->debugWaitQuorumStatus(true);
	tnodes[1]->debugWaitQuorumStatus(true);
	std::cerr << "Sending stop from the master controller" << std::endl;
	master->debugHalt();
	master=repocontrollermasterptr();

	tnodes[0]->wait(); // [HALTCLUSTERSTOP]
	std::cerr << "Node stopped" << std::endl;
}

static void test2(tstnodes &t)
{
	std::cerr << "test2" << std::endl;
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	t.startmastercontrolleron0_int(tnodes);

	tnodes[0]->debugWaitQuorumStatus(true);
	tnodes[1]->debugWaitQuorumStatus(true);

	nodeclusterstatus status=
		*clusterinfoObj::status(clusterinfo(tnodes[0]->repocluster));

	x::ref<repopeerconnectionObj>
		(tnodes[1]->repocluster->getnodepeer(tnodes[0]->listener
						     ->nodeName()))
		->debugHalt(status.master, status.uuid);
	tnodes[1]->wait(); // [HALTCLUSTERRECV]
	std::cerr << "Node stopped" << std::endl;
}

class haltnodeThread : virtual public x::obj {

public:
	std::string message;

	haltnodeThread() {}
	~haltnodeThread() noexcept {}

	void run(const STASHER_NAMESPACE::client &start_arg)
	{
		message=start_arg->haltrequest()->message;
	}
};

static void test3(tstnodes &t)
{
	std::cerr << "test3" << std::endl;
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	t.startmastercontrolleron0_int(tnodes);

	tnodes[0]->debugWaitQuorumStatus(true);
	tnodes[1]->debugWaitQuorumStatus(true);

	std::cout
		<< STASHER_NAMESPACE::client::base::admin(tstnodes
							  ::getnodedir(1))
		->haltrequest()->message
		<< std::endl;

	auto haltthread=x::ref<haltnodeThread>::create();

	auto haltrun=x::run(haltthread, STASHER_NAMESPACE::client
			    ::base::admin(tstnodes::getnodedir(0)));
	tnodes[1]->wait();
	haltrun->wait();
	std::cout << haltthread->message << std::endl;

	tnodes[0]->wait();
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

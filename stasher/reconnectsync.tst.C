/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "stasher/client.H"

#include <iostream>

void test1(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.setmasteron0(tnodes);

	std::cerr << "Starting first two nodes" << std::endl;

	tnodes[0]->start(true);
	tnodes[1]->start(true);
	tnodes[0]->debugWaitFullQuorumStatus(false);
	tnodes[1]->debugWaitFullQuorumStatus(false);
	std::cerr << "Waiting for nodes to connect to each other" << std::endl;
	tnodes[0]->listener->connectpeers();
	std::cerr << "Waiting for the first two nodes to form a quorum"
		  << std::endl;
	tnodes[0]->debugWaitMajorityQuorumStatus(true);
	tnodes[1]->debugWaitMajorityQuorumStatus(true);

	{
		STASHER_NAMESPACE::client cl0=
			STASHER_NAMESPACE::client::base
			::connect(tstnodes::getnodedir(0));

		STASHER_NAMESPACE::client::base::transaction tran=
			STASHER_NAMESPACE::client::base::transaction::create();

		tran->newobj("obj1", "obj1_value\n");
		STASHER_NAMESPACE::putresults res=cl0->put(tran);

		if (res->status != STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION(x::tostring(res->status));

		std::cerr << "Success putting obj1: "
			  << x::tostring(res->newuuid) << std::endl;
	}

	tnodes[2]->start(true);
	tnodes[2]->debugWaitFullQuorumStatus(false);
	std::cerr << "Started third node, connecting to peers"
		  << std::endl;
	tnodes[2]->listener->connectpeers();
	std::cerr << "Connecting to the third node" << std::endl;
	STASHER_NAMESPACE::client cl2=
		STASHER_NAMESPACE::client::base
		::connect(tstnodes::getnodedir(2));

	// The server will certainly get the get request
	// before it fully syncs with the master. This
	// tests that the get request gets properly
	// held until the slave is fully synced,
	// and the pending get request does not
	// block the sync from occurring.

	std::cerr << "Retrieving obj1" << std::endl;

	auto results=cl2->get("obj1", true)->objects;

	if (!results->succeeded)
		throw EXCEPTION(results->errmsg);

	auto p=results->find("obj1");

	if (p == results->end())
		throw EXCEPTION("Where's obj1?");

	std::string line;

	std::getline(*p->second.fd->getistream(), line);

	if (line != "obj1_value")
		throw EXCEPTION("Didn't get what we put?");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		tstnodes nodes(3);
		test1(nodes);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "repocontrollermaster.H"
#include <x/property_properties.H>

static void test1(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	t.startmastercontrolleron0(tnodes);
	t.any1canbemaster(tnodes);

	tnodes[0]->debugWaitFullQuorumStatus(false);
	tnodes[1]->debugWaitFullQuorumStatus(false);
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	x::property::load_property("objrepo::timeoutread", "2", true, false);

	try {
		tstnodes nodes(2);

		std::cout << "test1" << std::endl;
		test1(nodes);

		nodes.useencryption=true;
		std::cout << "test2" << std::endl;
		test1(nodes);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

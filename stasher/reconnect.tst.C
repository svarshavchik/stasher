/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "repocontrollermaster.H"
#include <x/options.H>
#include <x/destroycallbackflag.H>

static void dummytran(std::vector<tstnodes::noderef> &tnodes,
		      const std::string &name)
{
	for (size_t i=0; i<tnodes.size(); i++)
	{
		std::ostringstream objname;

		objname << name << i;

		newtran tr(tnodes[i]->repo->newtransaction());

		(*tr->newobj(objname.str())->getostream())
			<< "test" << std::flush;

		trandistributorObj::transtatus stat;
		x::ptr<x::obj> mcguffin;
		mcguffin=x::ptr<x::obj>::create();

		stat=tnodes[i]->distributor->newtransaction(tr, mcguffin);

		x::destroyCallbackFlag flag(x::destroyCallbackFlag::create());

		mcguffin->addOnDestroy(flag);

		mcguffin=x::ptr<x::obj>();
		flag->wait();
	}
}

static void test1(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

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

	std::cerr << "Starting nodes" << std::endl;

	for (size_t i=0; i<tnodes.size(); i++)
		tnodes[i]->start(false);

	for (size_t i=0; i<tnodes.size(); i++)
		tnodes[i]->debugWaitFullQuorumStatus(true);

	dummytran(tnodes, "test1.part1.");
	std::cerr << "Shutting down one node." << std::endl;

	tnodes[1]=tstnodes::noderef();
	tnodes[0]->debugWaitFullQuorumStatus(false);

	std::cerr << "Restarting node without autoconnect" << std::endl;

	tnodes[1]=new tstnodes::nodeObj(tstnodes::getnodedir(1));
	tnodes[1]->start(true);

	std::cerr << "Waiting for reconnect" << std::endl;
	tnodes[0]->debugWaitFullQuorumStatus(true);
	tnodes[0]->debugWaitFullQuorumStatus(true);
	dummytran(tnodes, "test1.part2.");
	std::cerr << "Done" << std::endl;
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	x::property::load_property(L"reconnect", L"4", true, true);

	try {
		tstnodes nodes(2);

		test1(nodes);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

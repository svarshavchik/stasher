/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "repocontrollermaster.H"
#include "clusterstatusnotifier.H"
#include "boolref.H"
#include "baton.H"
#include <x/destroy_callback.H>
#include <x/options.H>

class keeptrackofmasterObj : public clusterstatusnotifierObj {

public:
	std::vector<std::string> masterrecord;

	keeptrackofmasterObj() {}

	~keeptrackofmasterObj() {}

        void initialstatus(const nodeclusterstatus &newStatus)

	{
		masterrecord.push_back(newStatus.master);
	}

        void statusupdated(const nodeclusterstatus &newStatus)

	{
		if (masterrecord.back() == newStatus.master)
			return;

		masterrecord.push_back(newStatus.master);
	}
};

static void wait4quorum(const x::destroy_callback &cb,
			const boolref &status,
			std::vector<tstnodes::noderef> &tnodes)
{
	cb->wait();

	if (!status->flag) // [MASTERHANDOVERREQUEST]
		throw EXCEPTION("[MASTERHANDOVERREQUEST] failed");

	for (size_t i=0; i<tnodes.size(); i++)
		tnodes[i]->debugWaitFullQuorumStatus(true);
}

static void verifymaster(size_t whichone,
			 std::vector<tstnodes::noderef> &tnodes)
{
	std::string name=tnodes[whichone]->repocluster->getthisnodename();

	for (size_t i=0; i<tnodes.size(); ++i)
	{
		clusterinfo cluster=tnodes[i]->repocluster;

		if (clusterinfoObj::status(cluster)->master != name)
			throw EXCEPTION("[RESIGN] failed");

		batonptr batonp;

		while (!(batonp=cluster->getbaton()).null())
		{
			std::cerr << "Baton exists on node " << i
				  << ", waiting for it to go away" << std::endl;

			x::destroy_callback cb=
				x::destroy_callback::create();
			batonp->ondestroy([cb]{cb->destroyed();});
			batonp=batonptr();
			cb->wait();
		}
	}
}

static void test1(tstnodes &t, size_t n)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	t.startmastercontrolleron0(tnodes);
	t.startreconnecter(tnodes);
	t.any1canbemaster(tnodes);

	std::vector<x::ptr<keeptrackofmasterObj> > mastertrackers;

	for (size_t i=0; i<tnodes.size(); ++i)
	{
		x::ptr<keeptrackofmasterObj> t(x::ptr<keeptrackofmasterObj>
					       ::create());

		tnodes[i]->repocluster->installnotifyclusterstatus(t);

		mastertrackers.push_back(t);
	}

	boolref status=boolref::create();

	x::destroy_callback cb=x::destroy_callback::create();

	tnodes[n]->repocluster->
		master_handover_request(tstnodes::getnodefullname(1), status)
		->ondestroy([cb]{cb->destroyed();});

	wait4quorum(cb, status, tnodes);

	for (size_t i=0; i<mastertrackers.size(); ++i)
	{
		keeptrackofmasterObj &t= *mastertrackers[i];

		if (t.masterrecord.size() != 2 ||
		    t.masterrecord.front() != tstnodes::getnodefullname(0) ||
		    t.masterrecord.back() != tstnodes::getnodefullname(1))
			throw EXCEPTION("Master handover did not transition smoothly");
	}
	std::cout << "Test complete, shutting down" << std::endl;
}

static void test2(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0(tnodes);
	t.startreconnecter(tnodes);

	{
		std::set<size_t> nomasterset;

		nomasterset.insert(tnodes.size()-2);

		t.thesecantbemaster(tnodes, nomasterset);
	}

	for (size_t i=0; i<2; ++i)
	{
		std::cerr << "Asking node 0 to resign" << std::endl;

		boolref status=boolref::create();

		x::destroy_callback cb=x::destroy_callback::create();

		// [RESIGN]
		tnodes[0]->repocluster->resign(status)
			->ondestroy([cb]{cb->destroyed();});

		wait4quorum(cb, status, tnodes);

		verifymaster(tnodes.size()-1, tnodes);
	}

	{
		std::cerr << "Asking last node to resign" << std::endl;

		boolref status=boolref::create();

		x::destroy_callback cb=x::destroy_callback::create();

		tnodes.back()->repocluster->resign(status)
			->ondestroy([cb]{cb->destroyed();});

		wait4quorum(cb, status, tnodes);

		verifymaster(tnodes.size()-3, tnodes);
	}

}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	x::property::load_property("reconnect", "4", true, true);

	try {
		tstnodes nodes(6);

		for (size_t i=0; i<5; i++)
		{
			std::cout << "test 1, part " << i+1 << std::endl;
			test1(nodes, i);
		}

		std::cout << "test2" << std::endl;
		test2(nodes);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

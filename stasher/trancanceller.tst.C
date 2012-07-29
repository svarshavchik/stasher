/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"

#include <x/obj.H>
#include <x/uuid.H>

#define trandistributor_H

class trandistributorObj: virtual public x::obj {

public:
	trandistributorObj() {}
	~trandistributorObj() noexcept {}

	std::set<x::uuid> completeset;

	void completed(const x::uuid &uuid)
	{
		completeset.insert(uuid);
	}
};

#include "trancanceller.C"
#include "newtran.H"
#include "trancommit.H"

#include <x/options.H>
#include <x/dir.H>

static const char repo1[]="conftest1.dir";

void test1()
{
	tobjrepo repo(tobjrepo::create(repo1));
	x::ptr<trandistributorObj> fakedist(new trandistributorObj);

	x::ptr<trancancellerObj> canceller(new trancancellerObj(fakedist,
								"node1"));

	repo->installNotifier(canceller);

	x::uuid tran1, tran2;

	{
		newtran tr(repo->newtransaction());

		tr->newobj(std::string(tobjrepoObj::done_hier) + "/node1/"
			   + x::tostring(tran1));

		tr->newobj(std::string(tobjrepoObj::done_hier) + "/node2/"
			   + x::tostring(tran2));

		tr->newobj("foo/bar");

		repo->begin_commit(tr->finalize(), x::eventfd::create())
			->commit();
	}

	if (fakedist->completeset.size() != 1 ||
	    fakedist->completeset.find(tran1) == fakedist->completeset.end())
		throw EXCEPTION("[TRANCANCELLER] failed");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		x::dir::base::rmrf(repo1);

		ALARM(60);

		std::cout << "test1" << std::endl;
		test1();

		x::dir::base::rmrf(repo1);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

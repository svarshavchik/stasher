/*
** Copyright 2012-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"

#include <x/obj.H>
#include <x/uuid.H>

#define trandistributor_H

class trandistributorObj: virtual public x::obj {

public:
	trandistributorObj() {}
	~trandistributorObj() {}

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
#include <filesystem>

static const char repo1[]="conftest1.dir";

void test1()
{
	tobjrepo repo(tobjrepo::create(repo1));
	auto fakedist=x::ref<trandistributorObj>::create();

	auto canceller=x::ref<trancancellerObj>::create(fakedist, "node1");

	repo->installNotifier(canceller);

	x::uuid tran1, tran2;

	{
		newtran tr(repo->newtransaction());

		tr->newobj(std::string(tobjrepoObj::done_hier) + "/node1/"
			   + x::to_string(tran1));

		tr->newobj(std::string(tobjrepoObj::done_hier) + "/node2/"
			   + x::to_string(tran2));

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
		std::filesystem::remove_all(repo1);

		ALARM(60);

		std::cout << "test1" << std::endl;
		test1();

		std::filesystem::remove_all(repo1);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

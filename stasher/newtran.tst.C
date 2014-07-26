/*
** Copyright 2012-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "newtranfwd.H"
#include "tobjrepofwd.H"
#include <x/obj.H>
#include <x/ref.H>
#include <x/fd.H>
#include <x/uuid.H>
#include <x/logger.H>
#include <x/options.H>
#include <vector>
#include <unistd.h>

class tobjrepoObj : virtual public x::obj {

public:
	tobjrepoObj()
	{
	}

	~tobjrepoObj() noexcept
	{
	}

	std::vector<size_t> v;

	x::fd tmp_create(const x::uuid &dummy, size_t n)
	{
		v.push_back(n);
		return x::fd::base::tmpfile();
	}

	std::vector<size_t> r;

	void tmp_remove(const x::uuid &dummy, size_t n)
	{
		r.push_back(n);
	}

	void finalize(const newtran &dummy)
	{
	}

	x::fd tmp_reopen(const x::uuid &dummy, size_t n)
	{
		x::fd f(x::fd::base::tmpfile());

		(*f->getostream()) << "foo" << std::flush;

		return f;
	}
};

#define tobjrepo_H
#include "newtran.C"

#include <x/exception.H>

static void test1()
{
	tobjrepo r(tobjrepo::create());

	{
		newtran n(newtran::create(r));

		n->newobj("dummy1");
		n->delobj("dummy2", x::uuid());
		n->updobj("dummy3", x::uuid());

		n->finalize();
	}

	if (r->r.size() != 0 || r->v.size() != 2 ||
	    r->v[0] != 0 ||
	    r->v[1] != 2)
		throw EXCEPTION("newtran test1 failed");
}

static void test2()
{
	tobjrepo r(tobjrepo::create());

	{
		auto n=newtran::create(r);

		n->newobj("dummy1");
		n->delobj("dummy2", x::uuid());
		n->updobj("dummy3", x::uuid());

		newtranObj::objsizes_t objsizes;

		n->objsizes(objsizes);

		if (objsizes.size() != 2 ||
		    objsizes.find("dummy1") == objsizes.end() ||
		    objsizes.find("dummy3") == objsizes.end() ||
		    objsizes["dummy1"] != 3 ||
		    objsizes["dummy3"] != 3)
			throw EXCEPTION("newtran test3 failed");

	}

	if (r->r.size() != 3 ||
	    r->r[0] != 0 ||
	    r->r[1] != 1 ||
	    r->r[2] != 2)
		throw EXCEPTION("newtran test2 failed");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	ALARM(30);
	try {
		test1();
		test2();
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

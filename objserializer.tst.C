/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "objserializer.H"
#include "newtran.H"
#include "trancommit.H"

#include <x/serialize.H>
#include <x/deserialize.H>
#include <x/options.H>

#include <iostream>
#include <iterator>
#include <filesystem>

static void test1()
{
	std::filesystem::remove_all("conftest.dir");
	std::filesystem::remove_all("conftest2.dir");

	tobjrepo repo1(tobjrepo::create("conftest.dir"));

	x::uuid foouuid;

	{
		newtran t(repo1->newtransaction());

		x::fd foo(t->newobj("foo"));

		(*foo->getostream()) << "bar" << std::endl << std::flush;

		foo->close();

		foouuid=t->finalize();
	}

	repo1->begin_commit(foouuid, x::eventfd::create())->commit();

	std::vector<char> serbuf;

	{
		typedef std::back_insert_iterator< std::vector<char>
						   > ins_iter_t;

		ins_iter_t ins_iter(serbuf);

		x::serialize::iterator<ins_iter_t>
			ser(ins_iter);

		{
			objserializer serfoo(repo1, "foo", x::ptr<x::obj>());
			objserializer serbar(repo1, "bar", x::ptr<x::obj>());

			ser(serfoo);
			ser(serbar);
		}
	}

	tobjrepo repo2(tobjrepo::create("conftest2.dir"));

	{
		newtran t(repo2->newtransaction());

		x::fd foo(t->newobj("bar"));

		(*foo->getostream()) << "bar" << std::endl << std::flush;

		foo->close();

		x::uuid uuid(t->finalize());

		repo2->begin_commit(uuid, x::eventfd::create())->commit();
	}

	std::set<std::string> names;
	objrepoObj::values_t values;
	std::set<std::string> notfound;

	names.insert("foo");
	names.insert("bar");

	repo2->values(names, false, values, notfound);

	if (values.size() != 1 ||
	    values.begin()->first != "bar" ||
	    notfound.size() != 1 ||
	    *notfound.begin() != "foo")
		throw EXCEPTION("I screwed up");

	{
		std::vector<char>::const_iterator
			b(serbuf.begin()), e(serbuf.end());

		x::deserialize::iterator
			< std::vector<char>::const_iterator > deser(b, e);

		while (b != e)
		{
			objserializer obj=
				objserializer(tobjrepoptr(),
					      "dummy"); // [DISCARD]

			deser(obj);
		}
	}

	std::vector<char>::const_iterator
		b(serbuf.begin()), e(serbuf.end());

	x::deserialize::iterator
		< std::vector<char>::const_iterator > deser(b, e);

	while (b != e)
	{
		objserializer obj(repo2, "dummy");

		deser(obj); // [DIRECTCOMMIT]
	}

	values.clear();
	notfound.clear();

	repo2->values(names, true, values, notfound);

	if (values.size() == 1 && notfound.size() == 1 &&
	    *notfound.begin() == "bar") // [DIRECTREMOVE]
	{
		objrepoObj::values_t::iterator p(values.begin());

		if (p->first == "foo" && p->second.first == foouuid)
		{
			x::istream i(p->second.second->getistream());

			std::string s;

			std::getline(*i, s);

			if (s == "bar")
				return;
		}
	}

	throw EXCEPTION("Sanity check failed");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	ALARM(30);
	try {
		test1();
		std::filesystem::remove_all("conftest.dir");
		std::filesystem::remove_all("conftest2.dir");
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

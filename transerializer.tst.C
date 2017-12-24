/*
** Copyright 2012-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "transerializer.H"
#include "tobjrepo.H"
#include "trancommit.H"

#include <x/options.H>
#include <x/serialize.H>
#include <x/deserialize.H>

static const char repo1[]="conftest1.dir";
static const char repo2[]="conftest2.dir";

static void test1()
{
	tobjrepo one(tobjrepo::create(repo1));
	tobjrepo two(tobjrepo::create(repo2));

	x::uuid uuid;

	{
		newtran tran(two->newtransaction());

		(*tran->newobj("obj1")->getostream()) << "obj1" << std::endl;
		(*tran->newobj("obj2")->getostream()) << "obj2" << std::endl;

		two->begin_commit(uuid=tran->finalize(), x::eventfd::create())
			->commit();
	}

	{
		newtran tran(one->newtransaction());

		tran->delobj("obj1", uuid);
		(*tran->updobj("obj2", uuid)->getostream()) << "obj22"
							    << std::endl;

		(*tran->newobj("obj3")->getostream()) << "obj3" << std::endl;

		uuid=tran->finalize();
	}

	std::vector<char> buf;

	{
		typedef std::back_insert_iterator<std::vector<char> >
			ins_iter_t;

		ins_iter_t ins_iter(buf);

		x::serialize::iterator<ins_iter_t>
			ser_iter(ins_iter);

		transerializer serializer(transerializer::ser(&one, &uuid));

		ser_iter(serializer);
	}

	{
		typedef std::vector<char>::iterator iter_t;

		iter_t b(buf.begin()), e(buf.end());

		x::deserialize::iterator<iter_t>
			deser_iter(b, e);

		transerializer deserializer(two, spacemonitor::create
					    (x::df::create(".")));

		deser_iter(deserializer);
		deserializer.tran->finalize();
	}

	two->begin_commit(uuid, x::eventfd::create())->commit();

        tobjrepoObj::values_t valuesMap;

        std::set<std::string> notfound;
        {
                std::set<std::string> s;

                s.insert("obj1");
                s.insert("obj2");
                s.insert("obj3");

                two->values(s, true, valuesMap, notfound);
        }

	if (notfound.size() != 1 || notfound.find("obj1") == notfound.end())
		throw EXCEPTION("obj1 was not deleted");

	std::string s;
	if (valuesMap.find("obj2") == valuesMap.end() ||
	    (std::getline(*valuesMap.find("obj2")->second.second
			  ->getistream(), s), s) != "obj22")
		throw EXCEPTION("obj2 was not updated");

	if (valuesMap.size() != 2 ||
	    valuesMap.find("obj3") == valuesMap.end() ||
	    (std::getline(*valuesMap.find("obj3")->second.second
			  ->getistream(), s), s) != "obj3")
		throw EXCEPTION("obj3 was not inserted");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		x::dir::base::rmrf(repo1);
		x::dir::base::rmrf(repo2);

		ALARM(60);

		std::cout << "test1" << std::endl;
		test1();

		x::dir::base::rmrf(repo1);
		x::dir::base::rmrf(repo2);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

/*
** Copyright 2012-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include <string>
#include <unistd.h>

#include "objuuidenumerator.C"
#include "objuuidlist.C"
#include "objsink.C"
#include "objrepo.C"
#include "tobjrepo.C"
#include "newtran.C"
#include "tran.C"
#include "trancommit.C"
#include "writtenobjbase.C"
#include "objwriter.C"
#include <sstream>

#include <x/deserialize.H>
#include <x/options.H>

class uenum : public objuuidenumeratorObj {

public:
	size_t batchcnt, msgcnt;

	uenum(const tobjrepo &repo)
		: objuuidenumeratorObj(repo), batchcnt(0), msgcnt(0)
	{
	}

	~uenum() noexcept
	{
	}

private:
	// [NEXTBATCH]

	objuuidlist
	nextbatch(const std::set<std::string> &objectnames)

	{
		objuuidlist ret;

		if (objectnames.empty())
		{
			ret=objuuidenumeratorObj::nextbatch(objectnames);
		}
		else
		{
			tobjrepoObj::lockentry_t
				l(repo->lock(objectnames,
					     x::eventfd::create()));

			if (!l->locked())
				throw EXCEPTION("Lock is not acquired, as expected");

			ret=objuuidenumeratorObj::nextbatch(objectnames);
		}

		if (!ret.null())
			++batchcnt;

		return ret;
	}

	// [GETOBJECT]
	objuuidlist getobjuuidlist()
	{
		++msgcnt;

		return objuuidlist::create();
	}
};

static x::uuid createobjects(const tobjrepo &repo, size_t n,
			     std::set<std::string> &objnames)
{
	newtran tr(repo->newtransaction());

	for (size_t i=0; i<n; ++i)
	{
		std::ostringstream o;

		o << "obj" << i;

		tr->newobj(o.str());
		objnames.insert(o.str());
	}

	return tr->finalize();
}

static void test1(size_t n)
{
	x::dir::base::rmrf("conftest.dir");

	tobjrepo repo(tobjrepo::create("conftest.dir"));

	std::set<std::string> set1, set2;

	x::uuid uuid(createobjects(repo, n, set1));

	repo->begin_commit(uuid, x::eventfd::create())->commit();

	size_t batchcnt=0;

	{
		auto e=x::ref<uenum>::create(repo);

		// [ENUMERATE]

		objuuidlist ptr;

		while (!(ptr=e->next()).null())
		{
			for (std::map<std::string, x::uuid>
				     ::const_iterator
				     b(ptr->objuuids.begin()),
				     e(ptr->objuuids.end()); b != e; ++b)
			{
				if (b->second != uuid)
					throw EXCEPTION("object " + b->first +
							" has a strange uuid");
				set2.insert(b->first);
			}
			++batchcnt;
		}

		if (batchcnt != e->batchcnt ||
		    batchcnt != e->msgcnt)
			throw EXCEPTION("batchcnt mismatch");
	}

	std::cout << "Received " << batchcnt << " chunks" << std::endl;

	if (set1 != set2)
		throw EXCEPTION("Sanity check failed");

	std::cout << "Restored " << set2.size() << " uuids" << std::endl;
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"
	ALARM(30);
	try {
		// [PARTIAL]

		test1(objuuidlistObj::default_chunksize.getValue()-1);
		test1(objuuidlistObj::default_chunksize.getValue());
		test1(objuuidlistObj::default_chunksize.getValue()+1);
		test1(0);
		x::dir::base::rmrf("conftest.dir");
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

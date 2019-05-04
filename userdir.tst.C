/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "stasher/client.H"
#include <x/options.H>

static void test1(tstnodes &t)
{
	std::cerr << "test1" << std::endl;

	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	t.startmastercontrolleron0_int(tnodes);

	STASHER_NAMESPACE::client
		cl0=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));
	STASHER_NAMESPACE::client
		cl1=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(1));

	for (size_t i=0; i<25; ++i)
	{
		std::ostringstream o;

		o << "tst/obj" << i;

		std::cerr << i+1 << " objects" << std::endl;

		{
			STASHER_NAMESPACE::client::base::transaction tran=
				STASHER_NAMESPACE::client::base::transaction
				::create();

			tran->newobj(o.str(), o.str());

			STASHER_NAMESPACE::putresults res=cl0->put(tran);

			if (res->status !=
			    STASHER_NAMESPACE::req_processed_stat)
				throw EXCEPTION(x::to_string(res->status));
		}

		STASHER_NAMESPACE::getdirresults res=cl1->getdir("tst");

		if (res->objects.size() != i+1)
			throw EXCEPTION("[CLIENTGETDIR] failed (1)");

		for (size_t j=0; j <= i; ++j)
		{
			std::ostringstream o;

			o << "tst/obj" << j;

			if (res->objects.find(o.str()) == res->objects.end())
				throw EXCEPTION("[CLIENTDIR] failed (2)");
		}
	}

	{
		STASHER_NAMESPACE::getdirresults res=cl1->getdir("");

		if (res->objects.size() != 2 ||
		    res->objects.find("etc/") == res->objects.end() ||
		    res->objects.find("tst/") == res->objects.end())
			throw EXCEPTION("[CLIENTGETDIR] failed (3)");
	}

	{
		STASHER_NAMESPACE::client::base::transaction tran=
			STASHER_NAMESPACE::client::base::transaction
			::create();

		tran->newobj("tst/obj0/obj0", "dummy");

		STASHER_NAMESPACE::putresults res=cl0->put(tran);

		if (res->status != STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION(x::to_string(res->status));
	}

	STASHER_NAMESPACE::getdirresults res=cl1->getdir("tst");

	if (res->objects.size() != 26 ||
	    res->objects.find("tst/obj0/") == res->objects.end())
		throw EXCEPTION("[CLIENTGETDIR] failed (4)");

	for (size_t j=0; j < 25; ++j)
	{
		std::ostringstream o;

		o << "tst/obj" << j;

		if (res->objects.find(o.str()) == res->objects.end())
			throw EXCEPTION("[CLIENTDIR] failed (5)");
	}
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

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

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tranmeta.H"

#include <x/exception.H>
#include <x/serialize.H>
#include <x/deserialize.H>
#include <x/options.H>
#include <iostream>
#include <iterator>
#include <vector>
#include <cstdlib>
#include <unistd.h>

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	ALARM(30);
	try {
		x::uuid sv1, sv2;

		std::vector<char> tranmeta_serialized;

		tranmeta tm1;

		{
			tm1.newobj("obj0");
			tm1.updobj("obj1", sv1);
			tm1.delobj("obj2", sv2);
			tm1.opts["foo"]="bar";

			std::back_insert_iterator<std::vector<char> >
				ins_iter(tranmeta_serialized);

			x::serialize::iterator
				<std::back_insert_iterator<std::vector<char> >
				 > ser(ins_iter);

			ser(tm1);
		}

		{
			tranmeta tm2;

			std::vector<char>::const_iterator
				b(tranmeta_serialized.begin()),
				e(tranmeta_serialized.end());

			x::deserialize
				::iterator< std::vector<char>::const_iterator >
				deser(b, e);

			deser(tm2);

			if (tm1 != tm2)
				throw EXCEPTION("serialization failure");
		}
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "maxintsize.h"

#include <x/exception.H>
#include <x/options.H>
#include <x/property_value.H>

#include <sstream>
#include <iostream>
#include <cstdlib>

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		std::ostringstream a, b, c;

		a << ULLONG_MAX;
		b << LLONG_MAX;
		c << LLONG_MIN;

		if (a.str().size() >= MAXINTSIZE ||
		    b.str().size() >= MAXINTSIZE ||
		    c.str().size() >= MAXINTSIZE)
			throw EXCEPTION(({std::ostringstream o;

						o << "MAXINTSIZE(" << MAXINTSIZE
						  << ") is wrong";
						o.str();}));

	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

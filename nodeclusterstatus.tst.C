/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "nodeclusterstatus.H"
#include <x/options.H>

#include <iostream>
#include <cstdlib>

#define TST(NOT, val1, op, val2) do {					\
		nodeclusterstatus a=nodeclusterstatus val1,		\
			b=nodeclusterstatus val2;			\
									\
		if ( !(NOT (a op b)))					\
			throw EXCEPTION("Error: nodeclusterstatus" # val1 \
					" is not " # op			\
					" nodeclusterstatus" #val2 );	\
	} while (0)

#define s(a,b,c)( a, x::uuid(), b, c)

static void test1() // [COMPARE]
{
	TST(  , s("a", 0, false), ==, s("a", 0, false));
	TST( !, s("a", 0, false), ==, s("a", 1, false));
	TST(  , s("a", 0, false), !=, s("a", 1, false));
	TST( !, s("a", 0, false), !=, s("a", 0, false));

	TST(  , s("a", 1, false), >, s("a", 0, false));
	TST(  , s("a", 1, false), >, s("b", 0, false));
	TST(  , s("b", 1, false), >, s("a", 0, false));

	TST( !, s("a", 0, false), >, s("a", 0, false));
	TST( !, s("a", 0, false), >, s("a", 1, false));
	TST( !, s("b", 0, false), >, s("a", 1, false));
	TST( !, s("a", 0, false), >, s("b", 1, false));

	TST(  , s("a", 1, false), >=, s("a", 0, false));
	TST(  , s("a", 1, false), >=, s("b", 0, false));
	TST(  , s("b", 1, false), >=, s("a", 0, false));
	TST(  , s("a", 0, false), >=, s("a", 0, false));

	TST( !, s("a", 0, false), >=, s("a", 1, false));
	TST( !, s("b", 0, false), >=, s("a", 1, false));
	TST( !, s("a", 0, false), >=, s("b", 1, false));

	TST(  , s("a", 0, false), <, s("a", 1, false));
	TST(  , s("b", 0, false), <, s("a", 1, false));
	TST(  , s("a", 0, false), <, s("b", 1, false));

	TST( !, s("a", 1, false), <, s("a", 0, false));
	TST( !, s("a", 1, false), <, s("b", 0, false));
	TST( !, s("b", 1, false), <, s("a", 0, false));
	TST( !, s("a", 0, false), <, s("a", 0, false));

	TST(  , s("a", 0, false), <=, s("a", 1, false));
	TST(  , s("b", 0, false), <=, s("a", 1, false));
	TST(  , s("a", 0, false), <=, s("b", 1, false));
	TST(  , s("a", 0, false), <=, s("a", 0, false));

	TST( !, s("a", 1, false), <=, s("a", 0, false));
	TST( !, s("a", 1, false), <=, s("b", 0, false));
	TST( !, s("b", 1, false), <=, s("a", 0, false));
}

static void test2() // [COMPARE]
{
	TST( !, s("a", 0, false), ==, s("a", 0, true));
	TST( !, s("a", 0, false), ==, s("a", 1, true));
	TST(  , s("a", 0, false), !=, s("a", 1, true));
	TST(  , s("a", 0, false), !=, s("a", 0, true));

	TST(  , s("a", 1, false), >, s("a", 0, true));
	TST(  , s("a", 1, false), >, s("b", 0, true));
	TST(  , s("b", 1, false), >, s("a", 0, true));

	TST(  , s("a", 0, false), >, s("a", 0, true));
	TST(  , s("a", 0, false), >, s("a", 1, true));
	TST(  , s("b", 0, false), >, s("a", 1, true));
	TST(  , s("a", 0, false), >, s("b", 1, true));

	TST(  , s("a", 1, false), >=, s("a", 0, true));
	TST(  , s("a", 1, false), >=, s("b", 0, true));
	TST(  , s("b", 1, false), >=, s("a", 0, true));
	TST(  , s("a", 0, false), >=, s("a", 0, true));

	TST(  , s("a", 0, false), >=, s("a", 1, true));
	TST(  , s("b", 0, false), >=, s("a", 1, true));
	TST(  , s("a", 0, false), >=, s("b", 1, true));

	TST( !, s("a", 0, false), <, s("a", 1, true));
	TST( !, s("b", 0, false), <, s("a", 1, true));
	TST( !, s("a", 0, false), <, s("b", 1, true));

	TST( !, s("a", 1, false), <, s("a", 0, true));
	TST( !, s("a", 1, false), <, s("b", 0, true));
	TST( !, s("b", 1, false), <, s("a", 0, true));
	TST( !, s("a", 0, false), <, s("a", 0, true));

	TST( !, s("a", 0, false), <=, s("a", 1, true));
	TST( !, s("b", 0, false), <=, s("a", 1, true));
	TST( !, s("a", 0, false), <=, s("b", 1, true));
	TST( !, s("a", 0, false), <=, s("a", 0, true));

	TST( !, s("a", 1, false), <=, s("a", 0, true));
	TST( !, s("a", 1, false), <=, s("b", 0, true));
	TST( !, s("b", 1, false), <=, s("a", 0, true));
}

static void test3() // [COMPARE]
{
	TST( !, s("a", 0, true), ==, s("a", 0, false));
	TST( !, s("a", 0, true), ==, s("a", 1, false));
	TST(  , s("a", 0, true), !=, s("a", 1, false));
	TST(  , s("a", 0, true), !=, s("a", 0, false));

	TST( !, s("a", 1, true), >, s("a", 0, false));
	TST( !, s("a", 1, true), >, s("b", 0, false));
	TST( !, s("b", 1, true), >, s("a", 0, false));

	TST( !, s("a", 0, true), >, s("a", 0, false));
	TST( !, s("a", 0, true), >, s("a", 1, false));
	TST( !, s("b", 0, true), >, s("a", 1, false));
	TST( !, s("a", 0, true), >, s("b", 1, false));

	TST( !, s("a", 1, true), >=, s("a", 0, false));
	TST( !, s("a", 1, true), >=, s("b", 0, false));
	TST( !, s("b", 1, true), >=, s("a", 0, false));
	TST( !, s("a", 0, true), >=, s("a", 0, false));

	TST( !, s("a", 0, true), >=, s("a", 1, false));
	TST( !, s("b", 0, true), >=, s("a", 1, false));
	TST( !, s("a", 0, true), >=, s("b", 1, false));

	TST(  , s("a", 0, true), <, s("a", 1, false));
	TST(  , s("b", 0, true), <, s("a", 1, false));
	TST(  , s("a", 0, true), <, s("b", 1, false));

	TST(  , s("a", 1, true), <, s("a", 0, false));
	TST(  , s("a", 1, true), <, s("b", 0, false));
	TST(  , s("b", 1, true), <, s("a", 0, false));
	TST(  , s("a", 0, true), <, s("a", 0, false));

	TST(  , s("a", 0, true), <=, s("a", 1, false));
	TST(  , s("b", 0, true), <=, s("a", 1, false));
	TST(  , s("a", 0, true), <=, s("b", 1, false));
	TST(  , s("a", 0, true), <=, s("a", 0, false));

	TST(  , s("a", 1, true), <=, s("a", 0, false));
	TST(  , s("a", 1, true), <=, s("b", 0, false));
	TST(  , s("b", 1, true), <=, s("a", 0, false));
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	ALARM(30);
	try {
		test1();
		test2();
		test3();
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

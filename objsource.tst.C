/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"

#include "objsource.H"

#include <x/serialize.H>
#include <x/options.H>
#include <x/property_value.H>

#include <iostream>
#include <cstdlib>

class dummymsg {

public:

	dummymsg() {}
	template<typename x> dummymsg(x &dummy) {}

	std::string str;

	template<typename ptr_type, typename iter_type>
	static void serialize(ptr_type ptr, iter_type &iter)
	{
		iter(ptr->str);
	}
};


class dummyhandlerObj : virtual public x::obj {

public:

	// [DESER]

	template<typename ser_type> static void classlist(ser_type &arg)

	{
		arg.template serialize<dummymsg>();
	}


	std::vector<std::string> desered;

	void deserialized(dummymsg &msg)
	{
		if (msg.str == "baz")
			throw x::stopexception(); // [RETURN]

		desered.push_back(msg.str);
	}

	dummyhandlerObj()
	{
	}

	~dummyhandlerObj() {}
};

static void test1()
{
	std::vector<char> dummybuffer;

	{
		typedef std::back_insert_iterator< std::vector<char> > ins_iter;
		typedef x::serialize::iterator<ins_iter>
			ser_iter;

		ins_iter iter(dummybuffer);

		ser_iter ser(iter);

		dummymsg msg;

		msg.str="foo";
		ser(msg);
		msg.str="bar";
		ser(msg);
	}

	x::ptr<dummyhandlerObj> h(x::ptr<dummyhandlerObj>::create());

	std::vector<char>::iterator b=dummybuffer.begin(),
		e=dummybuffer.end();

	STASHER_NAMESPACE::objsource(b, e, *h); // [RETURN]

	if (h->desered.size() != 2 ||
	    h->desered[0] != "foo" ||
	    h->desered[1] != "bar")
		throw EXCEPTION("sanity check failed");
}

static void test2()
{
	std::vector<char> dummybuffer;

	{
		typedef std::back_insert_iterator< std::vector<char> > ins_iter;
		typedef x::serialize::iterator<ins_iter>
			ser_iter;

		ins_iter iter(dummybuffer);

		ser_iter ser(iter);

		dummymsg msg;

		msg.str="foo";
		ser(msg);
		msg.str="baz";
		ser(msg);
		msg.str="bar";
		ser(msg);
	}

	x::ptr<dummyhandlerObj> h(x::ptr<dummyhandlerObj>::create());

	std::vector<char>::iterator b=dummybuffer.begin(),
		e=dummybuffer.end();

	STASHER_NAMESPACE::objsource(b, e, *h);

	if (h->desered.size() != 1 ||
	    h->desered[0] != "foo")
		throw EXCEPTION("sanity check failed (2)");
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

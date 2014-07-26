/*
** Copyright 2012-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"

#include "objwriter.H"

#include <x/deserialize.H>
#include <x/options.H>
#include <vector>

class test : public STASHER_NAMESPACE::objwriterObj {

public:

	test() : STASHER_NAMESPACE::objwriterObj(256) // [BUFFER]
	{
	}

	~test() noexcept
	{
	}

	std::vector<char> buffer;

	size_t flush(const char *ptr, size_t cnt)
	{
		if (cnt > 128)
			cnt=128;

		buffer.insert(buffer.end(), ptr, ptr+cnt);
		return cnt;
	}
};

static void test1()
{
	auto t=x::ref<test>::create();

	std::vector<char> dummy;

	for (size_t i=0; i<500; ++i)
		dummy.push_back(i);

	t->serialize(dummy); // [SERIALIZE]

	while (t->buffered())
		t->pubflush(); // [FLUSH]

	std::vector<char>::iterator b(t->buffer.begin()), e(t->buffer.end());

	x::deserialize::iterator<std::vector<char>::iterator>
		deser_iter(b, e);

	std::vector<char> dummy2;

	deser_iter(dummy2);

	if (dummy != dummy2)
		throw EXCEPTION("Round trip failed");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	ALARM(30);
	try {
		test1();
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "clusterlistener.H"
#include "stoppablethreadtracker.H"
#include "dirs.H"
#include "repomg.H"

#include <x/dir.H>
#include <x/options.H>

class testclusterlistenerObj : public clusterlistenerObj {

public:
	testclusterlistenerObj(const std::string &directoryArg)
		: clusterlistenerObj(directoryArg)
	{
	}

	~testclusterlistenerObj() noexcept
	{
	}

	void start_network(const x::fd &sock,
				   const x::sockaddr &addr)
;

	void start_privsock(const x::fd &sock,
				    const x::sockaddr &addr)
;

	void start_pubsock(const x::fd &sock,
				    const x::sockaddr &addr)
;
};

void testclusterlistenerObj::start_network(const x::fd &sock,
					   const x::sockaddr &addr)

{
	(*sock->getostream()) << "network" << std::endl << std::flush;
}

void testclusterlistenerObj::start_privsock(const x::fd &sock,
					    const x::sockaddr &addr)

{
	(*sock->getostream()) << "private" << std::endl << std::flush;
}

void testclusterlistenerObj::start_pubsock(const x::fd &sock,
					   const x::sockaddr &addr)

{
	(*sock->getostream()) << "public" << std::endl << std::flush;
}

static void test1(const char *clusterdir,
		  const char *nodedir)
{
	time_t now(time(NULL));

	repomg::clustkey_generate(clusterdir, "test",
				  now,
				  now + 365 * 24 * 60 * 60,
				  "rsa",
				  "medium",
				  "sha1");

	repomg::nodekey_generate(nodedir, clusterdir, "",
				 "node",
				 now, now + 7 * 24 * 60 * 60,
				 "medium",
				 "sha1");

	STASHER_NAMESPACE::stoppableThreadTrackerImpl
		threads(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create());

	threads->start(x::ref<testclusterlistenerObj>::create(nodedir));

	{
		bool caught=false;

		try {
			x::ptr<testclusterlistenerObj>::create(nodedir);
			// [PORTLOCK]

		} catch (const x::exception &e)
		{
			std::cerr << "Expected exception: " << e
				  << std::endl;
			caught=true;
		}

		if (!caught)
			throw EXCEPTION("[PORTLOCK] failed");
	}

	x::fd socket(x::httportmap::create()->
		     connect("node.test", getuid()));

	std::string line;

	std::getline(*socket->getistream(), line);

	if (line != "network")
		throw EXCEPTION("Did not get the expected response from a network connect");

	socket=x::netaddr::create(SOCK_STREAM, "file:" +
				  STASHER_NAMESPACE::pubsockname(nodedir))
		->connect();

	std::getline(*socket->getistream(), line);

	if (line != "public")
		throw EXCEPTION("Did not get the expected response from a public socket connect");

	socket=x::netaddr::create(SOCK_STREAM, "file:" +
				  STASHER_NAMESPACE::privsockname(nodedir))
		->connect();

	std::getline(*socket->getistream(), line);

	if (line != "private")
		throw EXCEPTION("Did not get the expected response from a private socket connect");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	static const char clusterdir[]="conftestcluster1.dir";
	static const char nodedir[]="conftestnode1.dir";

	ALARM(30);
	try {
		x::dir::base::rmrf(clusterdir);
		x::dir::base::rmrf(nodedir);

		test1(clusterdir, nodedir);

		x::dir::base::rmrf(clusterdir);
		x::dir::base::rmrf(nodedir);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

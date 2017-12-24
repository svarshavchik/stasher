#include <iostream>
#include <stasher/client.H>
#include <stasher/userinit.H>
#include <x/fmtsize.H>

#include <vector>
#include <string>
#include <sstream>

void simpleget(int argc, char **argv)
{
	stasher::client client=stasher::client::base::connect();

	stasher::userinit limits=client->getlimits();

	std::cerr << "Maximum "
		  << limits.maxobjects
		  << " objects, "
		  << x::fmtsize(limits.maxobjectsize)
		  << " aggregate object size, per transaction."
		  << std::endl
		  << "Maximum "
		  << limits.maxsubs
		  << " concurrent subscriptions." << std::endl;

	stasher::client::base::getreq req
		=stasher::client::base::getreq::create();

	req->openobjects=true;
	req->objects.insert(argv+1, argv+argc);

	stasher::contents contents=client->get(req)->objects;

	if (!contents->succeeded)
		throw EXCEPTION(contents->errmsg);

	for (int i=1; i<argc; ++i)
	{
		stasher::contents::base::map_t::iterator p=
			contents->find(argv[i]);

		std::cout << argv[i];

		if (p == contents->end())
		{
			std::cout << ": does not exist" << std::endl;
			continue;
		}

		std::cout << " (uuid " << x::tostring(p->second.uuid)
			  << "): ";

		x::istream is=p->second.fd->getistream();

		std::string firstline;

		std::getline(*is, firstline);
		std::cout << firstline << std::endl;
	}
}

int main(int argc, char **argv)
{
	try {
		simpleget(argc, argv);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}

	return 0;
}

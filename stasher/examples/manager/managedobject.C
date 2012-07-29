#include <stasher/client.H>
#include <stasher/manager.H>
#include <stasher/managedhierarchymonitor.H>

#include <x/destroycallbackflag.H>
#include <x/fditer.H>

#include <iterator>
#include <iostream>

// An example of a stasher::managedobjectObj implementation.

class mymanagedobjectObj : public stasher::managedobjectObj {

public:
	mymanagedobjectObj()
	{
	}

	~mymanagedobjectObj() noexcept
	{
	}

	// Invoked when the connection gets established, or breaks.

	void connection_update(stasher::req_stat_t status) override
	{
		std::cout << ("Connection update: " + x::tostring(status)
			      + "\n") << std::flush;
	}

	// Invoked when the object gets created or updated.

	// If the object exists at the time that the subscription got opened,
	// or after reestablishing a connection with the server, this callback
	// gives the object's uuid and its contents.

	void updated(const std::string &objname,
		     const x::uuid &uuid,
		     const x::fd &contents) override
	{
		std::cout << objname << " (" << x::tostring(uuid) << "):"
			  << std::endl;

		std::copy(x::fdinputiter(contents),
			  x::fdinputiter(),
			  std::ostreambuf_iterator<char>(std::cout));
		std::cout << std::endl;
	}

	// Invoked when the object gets removed.

	// This callback gets invoked if the object does not exist at the time
	// that the subscription got opened, or after reestablishing a
	// connection with the server.

	void removed(const std::string &objname) override
	{
		std::cout << objname << ": removed"
			  << std::endl;
	}
};

void monitor(const std::list<std::string> &objects)
{
	x::destroyCallbackFlag::base::guard guard;

	auto client=stasher::client::base::connect();

	guard(client);
	// When we return from this func, make sure this goes away.

	auto manager=stasher::manager::create(L"", "10 seconds");

	std::cout << "Starting subscriptions, press ENTER to stop"
		  << std::endl;

	auto monitor=x::ref<mymanagedobjectObj>::create();
	std::list<x::ref<x::obj> > mcguffins;

	for (auto &name:objects)
	{
		mcguffins.push_back(manager->
				    manage_object(client, name, monitor));
	}

	std::string dummy;

	std::getline(std::cin, dummy);
}

int main(int argc, char **argv)
{
	try {
		std::list<std::string> args(argv+1, argv+argc);

		monitor(args);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}

	return 0;
}

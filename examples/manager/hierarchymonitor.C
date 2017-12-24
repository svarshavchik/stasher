#include <stasher/client.H>
#include <stasher/manager.H>
#include <stasher/managedhierarchymonitor.H>

#include <x/destroycallbackflag.H>

// An example of a stasher::managedhierarchymonitorObj implementation.

class mymonitorObj : public stasher::managedhierarchymonitorObj {

public:

	std::string hierarchy;

	mymonitorObj(const std::string &hierarchyArg) : hierarchy(hierarchyArg)
	{
		if (hierarchy.size() == 0)
			hierarchy="[root]";
	}

	~mymonitorObj()
	{
	}

	// Invoked when the connection gets established, or breaks.

	void connection_update(stasher::req_stat_t status) override
	{
		std::cout << ("Connection update: " + x::tostring(status)
			      + "\n") << std::flush;
	}

	// Initial contents of the hierarchy follow, until the next enumerated
	// call.

	void begin() override
	{
		std::cout << ("=== Initial contents of "
			      + hierarchy + "\n") << std::flush;
	}

	// End of the initial contents of the hierarchy
	void enumerated()
	{
		std::cout << ("=== End of "
			      + hierarchy + "\n") << std::flush;
	}

	// An object in the hierarchy has been added or updated.
	// After enumerated(), this gets called whenever an object in the
	// hierarchy gets changed.

	void updated(const std::string &objname,
		     const x::uuid &objuuid)
	{
		std::cout << (objname + " (" + x::tostring(objuuid)
			      + ")\n") << std::flush;
	}

	// An object in the hierarchy has been removed. Typically follows
	// enumerated(), as needed.
	//
	// It's possible that:
	//
	// - removed() also gets called after begin() but before
	// enumerated(), referring to an object that's already been reported
	// as updated(), or for an object that has not been reported as
	// updated().
	//
	// - During enumeration, updated() gets called more than once with the
	// same objname and uuid.
	//
	// This can happen when a transaction updates the hierarchy while it
	// is getting enumerated.
	//
	// Applications that maintain an internal snapshot of the hierarchy
	// can implement begin() by clearing the internal snapshot, then
	// applying updated/removed callback, as they come in; then when
	// enumerated() gets received, the internal snapshot now matches what's
	// in the repository. Applications should not consider a removed()
	// for an object they do not have a previous update() to be an error
	// condition, neither an update() with the object name and uuid
	// unchanged.

	void removed(const std::string &objname)
	{
		std::cout << (objname + " (removed)\n") << std::flush;
	}
};

void monitor(const std::list<std::string> &hierarchies)
{
	x::destroyCallbackFlag::base::guard guard;

	auto client=stasher::client::base::connect();

	guard(client);
	// When we return from this func, make sure this goes away.

	auto manager=stasher::manager::create(L"", "10 seconds");

	std::cout << "Starting subscriptions, press ENTER to stop"
		  << std::endl;

	std::list<x::ref<mymonitorObj> > monitors;
	std::list<x::ref<x::obj> > mcguffins;

	for (auto &hierarchy:hierarchies)
	{
		auto monitor=x::ref<mymonitorObj>::create(hierarchy);

		monitors.push_back(monitor);
		mcguffins.push_back(manager->
				    manage_hierarchymonitor(client,
							    hierarchy,
							    monitor));
	}

	std::string dummy;

	std::getline(std::cin, dummy);
}

int main(int argc, char **argv)
{
	try {
		std::list<std::string> args(argv+1, argv+argc);

		if (args.empty())
			args.push_back("");

		monitor(args);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}

	return 0;
}

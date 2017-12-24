#include <iostream>
#include <stasher/client.H>
#include <x/destroycallbackflag.H>

class getCallbackObj : public x::destroyCallbackObj {

public:
	std::string name;
	stasher::getrequest res;

	getCallbackObj(const std::string &nameArg,
		       const stasher::getrequest &resArg)
		: name(nameArg), res(resArg)
	{
	}

	~getCallbackObj() {
	}

	void destroyed() noexcept override
	{
		stasher::contents contents=res->getmsg()->objects;

		if (!contents->succeeded)
		{
			std::cerr << "Error: " << x::tostring(contents->errmsg)
				  << std::endl;
			return;
		}

		auto iter=contents->find(name);

		if (iter == contents->end())
		{
			std::cout << name << " removed" << std::endl;
			return;
		}

		std::string line;

		std::getline(*iter->second.fd->getistream(), line);

		std::cout << name << ": " << line << std::endl;
	}
};

class mySubscriberObj : public stasher::client::base::subscriberObj {

public:
	stasher::client client;

	mySubscriberObj(const stasher::client &clientArg)
		: client(clientArg)
	{
	}

	~mySubscriberObj()
	{
	}

	void updated(const std::string &objname,
		     const std::string &suffix) override
	{
		std::string name=objname+suffix;

		stasher::client::base::getreq req
			=stasher::client::base::getreq::create();

		req->openobjects=true;
		req->objects.insert(name);

		std::pair<stasher::getrequest, stasher::client::base::request>
			res=client->get_request(req);

		res.second->mcguffin()
			->addOnDestroy(x::ref<getCallbackObj>
				       ::create(name, res.first));
	}
};

void monitor(int argc, char **argv)
{
	if (argc < 2)
		throw EXCEPTION("Usage: monitor {object}+");

	stasher::client client=stasher::client::base::connect();

	x::destroyCallbackFlag::base::guard subscriber_guard;

	auto subscriber=x::ref<mySubscriberObj>::create(client);

	subscriber_guard(subscriber);
	// Makes sure subscriber gets destroyed before client does, when this
	// function terminates

	std::list<x::ref<x::obj> > mcguffins;

	for (int i=1; i<argc; ++i)
	{
		stasher::subscriberesults
			res=client->subscribe(argv[i], subscriber);

		if (res->status != stasher::req_processed_stat)
			throw EXCEPTION(x::tostring(res->status));

		mcguffins.push_back(res->mcguffin);

		auto cancel_mcguffin=res->cancel_mcguffin; // NOT USED

		std::cout << "Subscribed to " << argv[i] << std::endl;
	}

	std::cout << "Monitoring these objects, press ENTER to stop"
		  << std::endl;

	std::string dummy;

	std::getline(std::cin, dummy);
}

int main(int argc, char **argv)
{
	try {
		monitor(argc, argv);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}

	return 0;
}

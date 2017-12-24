#include <iostream>
#include <stasher/client.H>
#include <stasher/serverstatuscallback.H>
#include <x/fmtsize.H>

#include <string>

class mycallbackObj : public stasher::serverstatuscallbackObj {

public:
	mycallbackObj() {}
	~mycallbackObj() {}

	void serverinfo(const stasher::userhelo &serverinfo) override
	{
		std::cout << "Connected to " << serverinfo.nodename
			  << " (cluster " << serverinfo.clustername << ")"
			  << std::endl;

		std::cout << "Maximum "
			  << serverinfo.limits.maxobjects
			  << " objects, "
			  << x::fmtsize(serverinfo.limits.maxobjectsize)
			  << " aggregate object size, per transaction."
			  << std::endl
			  << "Maximum "
			  << serverinfo.limits.maxsubs
			  << " concurrent subscriptions." << std::endl;
	}

	void state(const stasher::clusterstate &state) override
	{
		std::cout << "Current master: " << state.master
			  << std::endl;

		for (auto &node:state.nodes)
		{
			std::cout << "    Peer: " << node << std::endl;
		}

		std::cout << "Quorum: full="
			  << x::tostring(state.full)
			  << ", majority="
			  << x::tostring(state.majority) << std::endl;
	}
};


void serverstatussubscriber()
{
	stasher::client client=stasher::client::base::connect();

	std::cout << "Subscribing to server status, press ENTER to stop"
		  << std::endl;

	auto subscriber=x::ref<mycallbackObj>::create();

	stasher::subscribeserverstatusresults results=
		client->subscribeserverstatus(subscriber);

	std::cout << "Subscription status: "
		  << x::tostring(results->status)
		  << std::endl;

	x::ref<x::obj> mcguffin=results->mcguffin;

	x::ref<x::obj> cancel_mcguffin=results->cancel_mcguffin;

	std::string dummy;
	std::getline(std::cin, dummy);
}

int main()
{
	try {
		serverstatussubscriber();
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}

	return 0;
}

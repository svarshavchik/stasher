#include <iostream>
#include <stasher/client.H>
#include <x/mpobj.H>

#include <queue>

class mySubscriberObj : public stasher::client::base::subscriberObj {

public:
	mySubscriberObj() {}
	~mySubscriberObj() noexcept
	{
	}

	typedef x::mpcobj<std::queue<std::string> > queue_t;

	queue_t queue;

	void updated(const std::string &objname,
		     const std::string &suffix) override
	{
		queue_t::lock lock(queue);

		lock->push(objname+suffix);

		lock.notify_one();
	}

	std::string get()
	{
		queue_t::lock lock(queue);

		while (lock->empty())
			lock.wait();

		std::string s=lock->front();

		lock->pop();
		return s;
	}
};

void simplesubscribe(int argc, char **argv)
{
	if (argc < 2)
		throw EXCEPTION("Usage: simplesubscribe {object}+");

	stasher::client client=stasher::client::base::connect();

	auto subscriber=x::ref<mySubscriberObj>::create();

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

	while (1)
	{
		std::string object=subscriber->get();

		stasher::client::base::getreq req
			=stasher::client::base::getreq::create();

		req->objects.insert(object);
		req->openobjects=true;

		stasher::contents contents=client->get(req)->objects;

		if (!contents->succeeded)
			throw EXCEPTION(contents->errmsg);

		auto iter=contents->find(object);

		if (iter == contents->end())
		{
			std::cout << object << " removed" << std::endl;
			continue;
		}

		std::string line;

		std::getline(*iter->second.fd->getistream(), line);

		if (line == "stop")
			break;
		std::cout << object << ": " << line << std::endl;
	}
}

int main(int argc, char **argv)
{
	try {
		simplesubscribe(argc, argv);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}

	return 0;
}

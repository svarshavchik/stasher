#include <stasher/heartbeat.H>
#include <stasher/client.H>
#include <stasher/manager.H>
#include <x/threads/run.H>
#include <x/mpobj.H>
#include <x/ymdhms.H>
#include <iostream>
#include <deque>

// The identifier for each instance of the application.

// The identifier can be any class that:
// - Implements strict weak ordering using operator<(). Depending on other
// uses, it might also need to implement other comparison operators.
// - Has a default constructor
// - Serializable, using x::serialize/x::deserialize (this implies a default
// constructor)
// - Is stringable, using x::tostring

class application_id {

public:

	// For this example, the identifier consists of a node name and a pid.

	std::string nodename;
	pid_t pid;

	application_id()
	{
	}

	application_id(const std::string &nodenameArg, pid_t pidArg)
		: nodename(nodenameArg), pid(pidArg)
	{
	}

	// Implement the serialization requirement
	template<typename iter_type> void serialize(iter_type &iter)
	{
		iter(nodename);
		iter(pid);
	}

	// Implement strict weak ordering

	bool operator<(const application_id &o) const
	{
		return nodename < o.nodename ||
				  (nodename == o.nodename &&
				   pid < o.pid);
	}

	bool operator==(const application_id &o) const
	{
		return nodename == o.nodename && pid == o.pid;
	}

	// Implement stringability

	static const x::stringable_t stringable=x::class_tostring;

	template<typename OutputIterator>
	OutputIterator toString(OutputIterator iter,
				const x::const_locale &localeRef) const
	{
		std::ostringstream o;

		o << nodename << ", pid " << pid;

		std::string s=o.str();

		return std::copy(s.begin(), s.end(), iter);
	}
};

// Our heartbeat status is a plain std::string, but it can also be any
// arbitrary class that meets the same requirements as the node identifier,
// except for strict weak ordering.

typedef stasher::heartbeat<application_id, std::string> heartbeat;

// The heartbeat update thread

class updatethrObj : virtual public x::obj {

public:

	// Mutex-protected thread metadata
	class input_t {
	public:

		// Queue for heartbeat update requests.
		std::deque<heartbeat::base::update_type_t> update;

		// Main thread has terminated. Time to wrap things up
		bool eof;

		// Most recently posted heartbeat from all instances.
		// Keyed by instance name, value is the instance's metadata.
		// When this changes, we print it.
		std::map<application_id, std::string> current;

		// Our official heartbeat status, posted to the repository
		std::string my_status;
		input_t() : eof(false) {}
	};

	typedef x::mpcobj<input_t> input_meta_t;

	input_meta_t meta;

	// Retrieve the posted heartbeat status from all instances.
	std::map<application_id, std::string> getstatus(const heartbeat &hb)
	{
		std::map<application_id, std::string> m;

		heartbeat::base::lock lock(*hb);

		if (!lock->value.null()) // Empty, if no heartbeat object posted yet.
			for (auto &timestamp: lock->value->timestamps)
			{
				m.insert(std::make_pair(timestamp.first,
							timestamp.second.meta));
			}
		return m;
	}

	// The update thread

	void run(const heartbeat &hb)
	{
		while (1)
		{
			auto cur_value=getstatus(hb);

			input_meta_t::lock lock(meta);

			if (lock->eof)
				break;

			if (cur_value != lock->current)
			{
				lock->current=cur_value;

				std::cout << std::setw(79)
					  << std::setfill('-')
					  << "" << std::setw(0) << std::endl;

				// Posted heartbeat status has changed. Show it.
				for (auto &status:lock->current)
				{
					std::cout << x::tostring(status.first)
						  << ": "
						  << status.second
						  << std::endl;
				}
				std::cout << std::setw(79)
					  << std::setfill('-')
					  << "" << std::setw(0) << std::endl
					  << std::endl;
			}

			// Wait for an update request.
			if (lock->update.empty())
			{
				lock.wait();
				continue;
			}

			auto update_type=lock->update.front();
			lock->update.pop_front();

			// Pass the update request, and my current status, to
			// the heartbeat template.
			hb->update(update_type,

				   // Including the current timestamp in the
				   // message, below, results in the posted
				   // heartbeat status of each instance being
				   // different with every periodic refresh
				   // (ten seconds).

				   (lock->my_status.size()
				    ? lock->my_status:"(none)"));
		}
	}

	// The heartbeat template wants our current heartbeat status.

	void push(heartbeat::base::update_type_t type)
	{
		input_meta_t::lock lock(meta);

		lock->update.push_back(type);
		lock.notify_all();
	}

	// The main thread posts new official status.

	void push(const std::string &new_status)
	{
		input_meta_t::lock lock(meta);

		lock->update.push_back(heartbeat::base::app_update);
		lock->my_status=new_status;
		lock.notify_all();
	}
};

// A container for the heartbeat update thread's instance. This makes sure
// that the thread terminates before it goes out of scope.
//
// As explained, the heartbeat template instance must go out of scope and
// get destroyed before the client connect object, otherwise a deadlock may
// occur.

class thr_instance {

public:
	// The running thread
	x::runthread<void> run;

	// The object the thread is running.
	x::ref<updatethrObj> thr;

	// Start the thread
	thr_instance(const x::ref<updatethrObj> &thrArg,
		     const heartbeat &heartbeat)
		: run(x::run(thrArg, heartbeat)), thr(thrArg)
	{
	}

	// The destructor stops the thread.
	~thr_instance()
	{
		{
			updatethrObj::input_meta_t::lock lock(thr->meta);

			lock->eof=true;
			lock.notify_all();
		}

		run->get(); // Wait for the thread to stop
	}
};

void post_heartbeat()
{
	auto client=stasher::client::base::connect();

	std::string name=client->gethelo().nodename;

	if (name.empty())
		throw EXCEPTION("Not connected");

	std::cout << "Type (blindly) then ENTER to update this instance status, empty line to quit"
		  << std::endl;

	auto manager=stasher::manager::create();

	auto thr=x::ref<updatethrObj>::create();

	auto hb=heartbeat::create(manager, client,
				  "heartbeat", // Name of the object

				  application_id(name, getpid()),

				  // Our refresh interval
				  L"refresh",
				  std::chrono::seconds(10),

				  // Interval for purging out instances that
				  // no longer update.
				  L"stale",
				  std::chrono::seconds(30),


				  // Heartbeat template callback, requesting
				  // a posted update.
				  [thr]
				  (heartbeat::base::update_type_t update_type)
				  {
					  thr->push(update_type);
				  });

	thr_instance run1(thr, hb);

	// Starts everything, loops, posting the update.

	std::string line;

	while (!std::getline(std::cin, line).eof())
	{
		if (line.empty())
			break;
		thr->push(line);
	}
}

int main(int argc, char **argv)
{
	try {
		post_heartbeat();
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

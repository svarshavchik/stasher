/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "stasher/heartbeat.H"
#include <x/threads/run.H>
#include <x/mpobj.H>
#include <iostream>
#include <deque>

typedef STASHER_NAMESPACE::heartbeat<std::string, std::string> test1_hb;

template class STASHER_NAMESPACE::heartbeatObj<std::string, std::string>;
template class STASHER_NAMESPACE::heartbeatObj<int, std::string>;

class test1thrObj : virtual public x::obj {

public:

	class input_t {
	public:
		bool eof;
		std::deque<test1_hb::base::update_type_t> update;

		input_t() : eof(false) {}
	};

	typedef x::mpcobj<input_t> input_meta_t;

	input_meta_t meta;

	void run(const test1_hb &heartbeat,
		 const std::string &value)
	{
		input_meta_t::lock lock(meta);

		while (!lock->eof)
		{
			if (lock->update.empty())
			{
				lock.wait();
				continue;
			}

			auto update_type=lock->update.front();

			lock->update.pop_front();
			heartbeat->update(update_type, value);
		}
	}

	void push(test1_hb::base::update_type_t type)
	{
		input_meta_t::lock lock(meta);

		lock->update.push_back(type);
		lock.notify_all();
	}
};

// Container for the thread instance. Destructor kills the thread.

class test1thr_instance {

public:
	x::runthread<void> run;

	x::ref<test1thrObj> thr;

	test1thr_instance(const x::ref<test1thrObj> &thrArg,
			  const test1_hb &heartbeat,
			  const std::string &value)
		: run(x::run(thrArg, heartbeat, value)), thr(thrArg)
	{
	}

	~test1thr_instance()
	{
		{
			test1thrObj::input_meta_t::lock lock(thr->meta);

			lock->eof=true;
			lock.notify_all();
		}

		run->get();
	}
};

void test1(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	auto client1=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));
	auto client2=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	auto manager=STASHER_NAMESPACE::manager::create();

	auto thra=x::ref<test1thrObj>::create(),
		thrb=x::ref<test1thrObj>::create();


	auto hba=test1_hb::create(manager, client1,
				  "heartbeat",
				  "a",
				  L"refresh",
				  std::chrono::seconds(2),
				  L"stale",
				  std::chrono::seconds(3),
				  [thra]
				  (test1_hb::base::update_type_t update_type)
				  {
					  thra->push(update_type);
				  }),
		hbb=test1_hb::create(manager, client2,
				     "heartbeat",
				     "b",
				     L"refresh",
				     std::chrono::seconds(2),
				     L"stale",
				     std::chrono::seconds(3),
				     [thrb]
				     (test1_hb::base::update_type_t update_type)
				     {
					     thrb->push(update_type);
				     });

	test1thr_instance run1(thra, hba, "avalue");

	{
		test1thr_instance run2(thrb, hbb, "bvalue");

		std::cout << "Waiting for the heartbeat to be initialized"
			  << std::endl;

		{
			test1_hb::base::lock lock(*hba);

			lock.wait([&lock]
				  {
					  test1_hb::base::timestamps_t::iterator
						  a, b;

					  return !lock->value.null()
						  && (a=lock->value
						      ->timestamps.find("a"))
						      != lock->value->
						      timestamps.end()
						  && (b=lock->value
						      ->timestamps.find("b"))
						      != lock->value->
						      timestamps.end()
						  && a->second.meta == "avalue"
						  && b->second.meta == "bvalue";
				  });
			for (auto &timestamp:lock->value->timestamps)
			{
				std::cout << timestamp.first << ": "
					  << (std::string)x::ymdhms(timestamp
								    .second
								    .timestamp)
					  << ": "
					  << timestamp.second.meta
					  << std::endl;
			}
		}
	}

	std::cout << "Waiting for a stale heartbeat to be purged"
		  << std::endl;

	{
		test1_hb::base::lock lock(*hba);

		lock.wait([&lock]
			  {
				  test1_hb::base::timestamps_t::iterator b;

				  return !lock->value.null()
					  && lock->value->timestamps.find("a")
					  != lock->value->timestamps.end()
					  && lock->value->timestamps.find("b")
					  == lock->value->timestamps.end();
			  });
	}
}

void test2(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	auto client1=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));
	auto client2=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(1));

	auto manager1=STASHER_NAMESPACE::manager::create(),
		manager2=STASHER_NAMESPACE::manager::create();

	auto thra=x::ref<test1thrObj>::create(),
		thrb=x::ref<test1thrObj>::create();


	auto hba=test1_hb::create(manager1, client1,
				  "heartbeat",
				  "a",
				  L"refresh",
				  std::chrono::seconds(2),
				  L"stale",
				  std::chrono::seconds(3),
				  [thra]
				  (test1_hb::base::update_type_t update_type)
				  {
					  thra->push(update_type);
				  }),
		hbb=test1_hb::create(manager2, client2,
				     "heartbeat",
				     "b",
				     L"refresh",
				     std::chrono::seconds(2),
				     L"stale",
				     std::chrono::seconds(3),
				     [thrb]
				     (test1_hb::base::update_type_t update_type)
				     {
					     thrb->push(update_type);
				     });

	test1thr_instance run1(thra, hba, "avalue");

	{
		test1thr_instance run2(thrb, hbb, "bvalue");

		std::cout << "Waiting for the heartbeat to be initialized"
			  << std::endl;

		{
			test1_hb::base::lock lock(*hba);

			lock.wait([&lock]
				  {
					  test1_hb::base::timestamps_t::iterator
						  a, b;

					  return !lock->value.null()
						  && (a=lock->value
						      ->timestamps.find("a"))
						      != lock->value->
						      timestamps.end()
						  && (b=lock->value
						      ->timestamps.find("b"))
						      != lock->value->
						      timestamps.end()
						  && a->second.meta == "avalue"
						  && b->second.meta == "bvalue";
				  });
			for (auto &timestamp:lock->value->timestamps)
			{
				std::cout << timestamp.first << ": "
					  << (std::string)x::ymdhms(timestamp
								    .second
								    .timestamp)
					  << ": "
					  << timestamp.second.meta
					  << std::endl;
			}
		}
	}

	tnodes[1]=tstnodes::noderef();

	std::cout << "Staying down, for a few seconds..." << std::endl;

	sleep(10);

	tnodes[1]=tstnodes::noderef::create(tstnodes::getnodedir(1));
	tnodes[1]->start(true);

	std::cout << "Waiting to reconnect" << std::endl;
	tnodes[1]->debugWaitFullQuorumStatus(false);
	tnodes[1]->listener->connectpeers();
	tnodes[1]->debugWait4AllConnections();
	std::cerr << "Waiting for quorum" << std::endl;

	tnodes[0]->debugWaitFullQuorumStatus(true);
	tnodes[1]->debugWaitFullQuorumStatus(true);

	test1_hb::base::lock lock(*hba);

	time_t current_timestamp=lock->value->timestamps.find("a")
		->second.timestamp;

	std::cout << "Waiting for the first node to come alive"
		  << std::endl;

	lock.wait([&lock, &current_timestamp]
		  {
			  return lock->value->timestamps.find("a")
				  ->second.timestamp != current_timestamp;
		  });
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		{
			tstnodes nodes(1);
			test1(nodes);
		}

		{
			tstnodes nodes(2);
			test2(nodes);
		}
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "stasher/client.H"
#include "stasher/manager.H"
#include "stasher/managedobject.H"
#include "stasher/puttransaction.H"
#include <x/options.H>

class test1object : public STASHER_NAMESPACE::managedobjectObj {

public:
	std::map<std::string, std::pair<std::string, x::uuid> > managed;
	x::uuid uuid;
	STASHER_NAMESPACE::req_stat_t stat;
	bool stat_received;
	std::mutex mutex;
	std::condition_variable cond;

	test1object() : stat_received(false) {}
	~test1object() {}

	void connection_update(STASHER_NAMESPACE::req_stat_t statArg) override
	{
		std::unique_lock<std::mutex> lock(mutex);

		stat_received=true;
		stat=statArg;
		cond.notify_all();
	}

	void updated(const std::string &objname,
		     const x::uuid &uuid,
		     const x::fd &contents) override
	{
		std::string line;

		std::getline(*contents->getistream(), line);

		std::unique_lock<std::mutex> lock(mutex);

		managed[objname]=std::make_pair(line, uuid);
		cond.notify_all();
	}

	void removed(const std::string &objname) override
	{
		std::unique_lock<std::mutex> lock(mutex);

		managed.erase(objname);
		cond.notify_all();
	}
};


static void test1(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	auto client=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	x::uuid uuid=({
			auto put=STASHER_NAMESPACE::client::base::transaction::create();

			put->newobj("fyle", "Fyle\n");

			auto res=client->put(put);

			if (res->status != STASHER_NAMESPACE::req_processed_stat)
				throw EXCEPTION("PUT failed");

			res->newuuid;
		});

	auto manager=STASHER_NAMESPACE::manager::create();

	std::cerr << "Subscribing" << std::endl;

	x::ptr<test1object> rootsub=x::ptr<test1object>::create();

	auto mcguffin=manager->manage_object(client, "fyle", rootsub);

	{
		std::unique_lock<std::mutex> lock(rootsub->mutex);

		rootsub->cond.wait(lock, [&rootsub, &uuid] {
				auto iter=rootsub->managed.find("fyle");

				return iter != rootsub->managed.end() &&
					iter->second.first == "Fyle" &&
					iter->second.second == uuid;
			});
	}

	tnodes.clear();

	std::cerr << "Waiting for disconnected" << std::endl;

	{
		std::unique_lock<std::mutex> lock(rootsub->mutex);

		rootsub->cond
			.wait(lock, [&rootsub]
			      {
				      return rootsub->stat_received &&
					      rootsub->stat == STASHER_NAMESPACE::req_disconnected_stat;
			      });
		rootsub->managed.clear();
	}
	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	std::cerr << "Waiting for reconnection" << std::endl;
	{
		std::unique_lock<std::mutex> lock(rootsub->mutex);

		rootsub->cond
			.wait(lock, [&rootsub]
			      {
				      return rootsub->stat_received &&
					      rootsub->stat == STASHER_NAMESPACE::req_processed_stat;
			      });
		rootsub->managed.clear();
	}

	std::cerr << "Waiting for an update" << std::endl;
	{
		std::unique_lock<std::mutex> lock(rootsub->mutex);

		rootsub->cond.wait(lock, [&rootsub, &uuid] {
				auto iter=rootsub->managed.find("fyle");

				return iter != rootsub->managed.end() &&
					iter->second.first == "Fyle" &&
					iter->second.second == uuid;
			});
	}

	uuid=({
			auto put=STASHER_NAMESPACE::client::base::transaction::create();

			put->updobj("fyle", uuid, "newfyle\n");

			auto res=client->put(put);

			if (res->status != STASHER_NAMESPACE::req_processed_stat)
				throw EXCEPTION("PUT failed");

			res->newuuid;
		});

	std::cerr << "Waiting for another update" << std::endl;
	{
		std::unique_lock<std::mutex> lock(rootsub->mutex);

		rootsub->cond.wait(lock, [&rootsub, &uuid] {
				auto iter=rootsub->managed.find("fyle");

				return iter != rootsub->managed.end() &&
					iter->second.first == "newfyle" &&
					iter->second.second == uuid;
			});
	}

	uuid=({
			auto put=STASHER_NAMESPACE::client::base::transaction::create();

			put->delobj("fyle", uuid);

			auto res=client->put(put);

			if (res->status != STASHER_NAMESPACE::req_processed_stat)
				throw EXCEPTION("PUT failed");

			res->newuuid;
		});

	std::cerr << "Waiting for removal" << std::endl;
	{
		std::unique_lock<std::mutex> lock(rootsub->mutex);

		rootsub->cond.wait(lock, [&rootsub] {
				return rootsub->managed.find("fyle") ==
					rootsub->managed.end();
			});
	}
}

static void test2(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	auto client=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));
	auto manager=STASHER_NAMESPACE::manager::create();

	const char * const badnames[] = {"", "foo/", "foo//bar"};

	for (auto badname:badnames)
	{
		std::cerr << "Subscribing \"" << badname << "\"" << std::endl;

		x::ref<test1object> rootsub=x::ref<test1object>::create();

		auto mcguffin=manager->manage_object(client, badname, rootsub);

		{
			std::unique_lock<std::mutex> lock(rootsub->mutex);

			rootsub->cond.wait(lock, [&rootsub] {
					return rootsub->stat_received;
				});

			if (rootsub->stat != STASHER_NAMESPACE::req_badname_stat)
				throw EXCEPTION("Bad name wasn't rejected");
		}
	}
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		tstnodes nodes(1);

		x::property::load_property("objrepo::manager", "2 seconds",
					   true, true);
		std::cerr << "test1" << std::endl;

		test1(nodes);

		std::cerr << "test2" << std::endl;
		test2(nodes);

	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

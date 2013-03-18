/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "stasher/client.H"
#include "stasher/manager.H"
#include "stasher/managedhierarchymonitor.H"
#include <x/mpobj.H>
#include <x/options.H>

class test1subscriber : public STASHER_NAMESPACE::managedhierarchymonitorObj {

public:

	class metadata_t {

	public:
		bool stat_received;
		bool begin_called;
		bool enumerated_called;
		STASHER_NAMESPACE::req_stat_t status;

		std::map<std::string, x::uuid> objects;

		metadata_t()
			: stat_received(false),
			  begin_called(false),
			  enumerated_called(false) {}
	};

	typedef x::mpcobj<metadata_t> meta_t;

	meta_t meta;

	test1subscriber() {}
	~test1subscriber() noexcept {}

	void connection_update(STASHER_NAMESPACE::req_stat_t status)
	{
		meta_t::lock lock(meta);

		lock->stat_received=true;
		lock->status=status;
		lock.notify_all();
	}

	void begin()
	{
		meta_t::lock lock(meta);

		lock->begin_called=true;
		lock->enumerated_called=false;
		lock->objects.clear();
		lock.notify_all();
	}

	void enumerated()
	{
		meta_t::lock lock(meta);

		lock->enumerated_called=true;
		lock.notify_all();
	}

	void updated(const std::string &objname,
		     const x::uuid &objuuid)
	{
		meta_t::lock lock(meta);

		std::cout << "Updated " << objname << ":"
			  << x::tostring(objuuid)
			  << std::endl;
		lock->objects[objname]=objuuid;
		lock.notify_all();
	}

	void removed(const std::string &objname)
	{
		meta_t::lock lock(meta);

		std::cout << "Removed " << objname << std::endl;
		lock->objects.erase(objname);
		lock.notify_all();
	}
};

#include "stasher/userinit.H"

static void test1(tstnodes &t, const std::string &hier, const std::string &pfix)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	auto client=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	std::cerr << "Client supports " << client->getlimits().maxobjects
		  << " objects per transaction" << std::endl;


	x::uuid objuuid=({
			auto tran=STASHER_NAMESPACE::client::base
				::transaction::create();

			tran->newobj(pfix + "a", "a");
			tran->newobj(pfix + "b", "b");

			auto res=client->put(tran);

			if (res->status !=
			    STASHER_NAMESPACE::req_processed_stat)
				throw EXCEPTION("put: " +
						x::tostring(res->status));

			res->newuuid;
		});

	auto manager=STASHER_NAMESPACE::manager::create();

	auto tester=x::ref<test1subscriber>::create();

	auto mcguffin=manager->manage_hierarchymonitor(client, hier, tester);

	std::cerr << "Waiting for begin() to get called" << std::endl;

	{
		test1subscriber::meta_t::lock lock(tester->meta);

		lock.wait([&lock] { return lock->begin_called; });

		std::cerr << "Waiting to get the initial contents of the hierarchy (uuid="
			  << x::tostring(objuuid) << ")"
			  << std::endl;

		lock.wait([&lock] { return lock->enumerated_called; });


		auto a=lock->objects.find(pfix + "a");
		auto b=lock->objects.find(pfix + "b");

		auto e=lock->objects.end();

		if (!(a != e && b != e &&
		      a->second == objuuid &&
		      b->second == objuuid))
			throw EXCEPTION("[MANAGEDHIERMONENUM] failed (1)");
	}

	x::uuid objuuidc=({
			auto tran=STASHER_NAMESPACE::client::base
				::transaction::create();

			tran->newobj(pfix + "c", "c");
			tran->delobj(pfix + "b", objuuid);

			auto res=client->put(tran);

			if (res->status !=
			    STASHER_NAMESPACE::req_processed_stat)
				throw EXCEPTION("put: " +
						x::tostring(res->status));

			res->newuuid;
		});

	std::cerr << "Waiting to get updated contents"
		  << std::endl;
	{
		test1subscriber::meta_t::lock lock(tester->meta);

		lock.wait([&lock, &objuuid, &objuuidc, &pfix] {
				auto a=lock->objects.find(pfix + "a");
				auto b=lock->objects.find(pfix + "b");
				auto c=lock->objects.find(pfix + "c");

				auto e=lock->objects.end();

				return a != e && b == e && c != e &&
					a->second == objuuid &&
					c->second == objuuidc;
			}); // [MANAGEDHIERMONUPD]
	}

	tnodes.clear();

	std::cerr << "Waiting for disconnection" << std::endl;

	{
		test1subscriber::meta_t::lock lock(tester->meta);

		lock.wait([&lock] { return lock->stat_received &&
					lock->status == STASHER_NAMESPACE::req_disconnected_stat; });
		lock->objects.clear();
	}
	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	std::cerr << "Waiting for reconnection" << std::endl;

	{
		test1subscriber::meta_t::lock lock(tester->meta);

		lock.wait([&lock, &objuuid, &objuuidc, &pfix] {
				auto a=lock->objects.find(pfix + "a");
				auto b=lock->objects.find(pfix + "b");
				auto c=lock->objects.find(pfix + "c");

				auto e=lock->objects.end();

				return lock->begin_called &&
					a != e && b == e && c != e &&
					a->second == objuuid &&
					c->second == objuuidc;
			}); // [MANAGEDHIERMONRECONNECT]
	}
}

static void test2(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	auto client=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	size_t n=client->getlimits().maxobjects;

	std::set<std::string> names;

	for (size_t i=0; i<n*3; ++i)
	{
		auto tran=STASHER_NAMESPACE::client::base
			::transaction::create();

		std::ostringstream o;

		o << "dir/obj" << i;
		std::string n=o.str();

		names.insert(n);

		tran->newobj(n, n);

		auto res=client->put(tran);

		if (res->status !=
		    STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION("put: " + x::tostring(res->status));
	}

	auto manager=STASHER_NAMESPACE::manager::create();

	auto tester=x::ref<test1subscriber>::create();

	auto mcguffin=manager->manage_hierarchymonitor(client, "dir", tester);

	std::cerr << "Waiting for enumeration to complete" << std::endl;

	{
		test1subscriber::meta_t::lock lock(tester->meta);

		lock.wait([&lock] {
				return lock->begin_called
					&& lock->enumerated_called;
			});

		for (auto &name:names)
		{
			if (lock->objects.find(name) == lock->objects.end())
				throw EXCEPTION("[MANAGEDHIERMONENUM] failed (2)");
		}
	}
}

static void test3(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	auto client=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	auto manager=STASHER_NAMESPACE::manager::create();

	auto tester=x::ref<test1subscriber>::create();

	auto mcguffin=manager->manage_hierarchymonitor(client, "foo//bar", tester);

	{
		test1subscriber::meta_t::lock lock(tester->meta);

		lock.wait([&lock] {
				return lock->stat_received;
			});

		if (lock->status != STASHER_NAMESPACE::req_badname_stat)
			throw EXCEPTION("Did not received expected badname status");
	}
}

static void test4(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	auto client=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	auto manager=STASHER_NAMESPACE::manager::create(L"autoreconnect", 2);

	auto tester=x::ref<test1subscriber>::create();

	auto mcguffin=manager->manage_hierarchymonitor(client, "hiermon", tester);

	{
		test1subscriber::meta_t::lock lock(tester->meta);

		lock.wait([&lock] {
				return lock->stat_received;
			});

		if (lock->status != STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION("Did not receive expected status");
	}

	{
		test1subscriber::meta_t::lock lock(tester->meta);

		lock.wait([&lock] {
				return lock->enumerated_called;
			});
	}
	std::cout << "Enumerated" << std::endl;

	x::uuid objuuid=({
			auto tran=STASHER_NAMESPACE::client::base
				::transaction::create();

			tran->newobj("hiermon/1/a", "a");
			tran->newobj("hiermon/2/b", "b");

			auto res=client->put(tran);

			if (res->status !=
			    STASHER_NAMESPACE::req_processed_stat)
				throw EXCEPTION("put: " +
						x::tostring(res->status));

			res->newuuid;
		});

	std::cerr << "Waiting to get updated contents"
		  << std::endl;
	{
		test1subscriber::meta_t::lock lock(tester->meta);

		lock.wait([&lock, &objuuid] {
				auto a=lock->objects.find("hiermon/1/a");
				auto b=lock->objects.find("hiermon/2/b");

				auto e=lock->objects.end();

				return a != e && b != e &&
					a->second == objuuid &&
					b->second == objuuid;
			});
	}

	tnodes.clear();

	std::cerr << "Waiting for disconnection" << std::endl;

	{
		test1subscriber::meta_t::lock lock(tester->meta);

		lock.wait([&lock] {
				return lock->stat_received &&
					lock->status == STASHER_NAMESPACE::req_disconnected_stat;
			});
		lock->objects.clear();
	}
	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	std::cerr << "Waiting for reconnection" << std::endl;

	{
		test1subscriber::meta_t::lock lock(tester->meta);

		lock.wait([&lock, &objuuid] {
				auto a=lock->objects.find("hiermon/1/a");
				auto b=lock->objects.find("hiermon/2/b");

				auto e=lock->objects.end();

				return a != e && b != e &&
					a->second == objuuid &&
					b->second == objuuid;
			});
	}

}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		tstnodes nodes(3);

		x::property::load_property(L"objrepo::manager", L"2 seconds",
					   true, true);
		x::property::load_property(L"reconnect", L"4", true, true);

		std::cerr << "test1 (Part I)" << std::endl;

		test1(nodes, "", "");

		std::cerr << "test1 (Part II)" << std::endl;

		test1(nodes, "sandbox", "sandbox/");
		std::cerr << "test2" << std::endl;
		test2(nodes);
		std::cerr << "test3" << std::endl;
		test3(nodes);
		std::cerr << "test4" << std::endl;
		test4(nodes);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "stasher/current.H"
#include "stasher/puttransaction.H"
#include <x/options.H>
#include <x/fditer.H>
#include <x/destroy_callback.H>

class dummyObj : virtual public x::obj {

public:

	x::uuid uuid;
	std::string s;

	dummyObj() {}
	dummyObj(const x::uuid &uuidArg, const x::fd &fd) : uuid(uuidArg)
	{
		std::getline(*fd->getistream(), s);
		std::cout << "Deserialized " << s << std::endl;
	}

	dummyObj(const x::uuid &uuidArg) : uuid(uuidArg) {}
	~dummyObj() {
	}
};

typedef x::ref<dummyObj> dummy;

typedef x::ptr<dummyObj> dummyptr;

static void test1(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);
	t.startreconnecter(tnodes);

	x::destroy_callback::base::guard guard;

	auto client=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(1));

	guard(client);

	x::uuid uuid=({
			auto put=STASHER_NAMESPACE::client::base::transaction::create();

			put->newobj("obj", "value1\n");

			auto res=client->put(put);

			if (res->status != STASHER_NAMESPACE::req_processed_stat)
				throw EXCEPTION("PUT failed");

			res->newuuid;
		});

	auto manager=STASHER_NAMESPACE::manager::create();

	auto d=STASHER_NAMESPACE::current<dummyptr>::create();

	auto mcguffin=d->manage(manager, client, "obj");

	std::cerr << ": Waiting for the initial update"
		  << std::endl;

	{
		STASHER_NAMESPACE::current<dummyptr>::base
			::current_value_t::lock lock(d->current_value);

		lock.wait([&lock, &uuid]
			  {
				  return !lock->value.null() &&
					  lock->isinitial &&
					  lock->value->uuid == uuid &&
					  lock->value->s == "value1" &&
					  lock->connection_status ==
					  STASHER_NAMESPACE::req_processed_stat;
			  }
			  );
	}

	std::cerr << "Waiting for the next update"
		  << std::endl;

	uuid=({
			auto put=STASHER_NAMESPACE::client::base::transaction::create();

			put->updobj("obj", uuid, "value2\n");

			auto res=client->put(put);

			if (res->status != STASHER_NAMESPACE::req_processed_stat)
				throw EXCEPTION("PUT failed");

			res->newuuid;
		});

	{
		STASHER_NAMESPACE::currentObj<dummyptr>::current_value_t::lock lock(d->current_value);

		lock.wait([&lock, &uuid]
			  {
				  return !lock->value.null() &&
					  !lock->isinitial &&
					  lock->value->uuid == uuid &&
					  lock->value->s == "value2";
			  }
			  );
	}

	{
		auto put=STASHER_NAMESPACE::client::base::transaction::create();

		put->delobj("obj", uuid);

		auto res=client->put(put);

		if (res->status != STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION("PUT failed");
	}

	{
		STASHER_NAMESPACE::currentObj<dummyptr>::current_value_t::lock lock(d->current_value);

		lock.wait([&lock, &uuid]
			  {
				  return lock->value.null() &&
					  !lock->isinitial;
			  }
			  );
	}

	tnodes[1]=tstnodes::noderef();

	std::cerr << "Waiting for disconnect" << std::endl;

	{
		STASHER_NAMESPACE::currentObj<dummyptr>
			::current_value_t::lock lock(d->current_value);

		lock.wait([&lock, &uuid]
			  {
				  return lock->connection_status ==
					  STASHER_NAMESPACE::
					  req_disconnected_stat;
			  }
			  );
	}

	std::cerr << "Updating the object behind the scenes" << std::endl;

	uuid=({
			auto client0=STASHER_NAMESPACE::client::base
				::connect(tstnodes::getnodedir(0));


			auto put=STASHER_NAMESPACE::client::base::transaction::create();

			put->newobj("obj", "value3\n");

			auto res=client0->put(put);

			if (res->status != STASHER_NAMESPACE::req_processed_stat)
				throw EXCEPTION("PUT failed: " << x::tostring(res->status));

			res->newuuid;
		});

	tnodes[1]=tstnodes::noderef::create(tstnodes::getnodedir(1));
	tnodes[1]->start(false);

	std::cerr << "Waiting for reconnect" << std::endl;

	sleep(3);

	{
		STASHER_NAMESPACE::currentObj<dummyptr>
			::current_value_t::lock lock(d->current_value);

		lock.wait([&lock, &uuid]
			  {
				  return lock->connection_status ==
					  STASHER_NAMESPACE::
					  req_processed_stat;
			  }
			  );
	}

	std::cerr << "Waiting to receive the new value we missed" << std::endl;
	{
		STASHER_NAMESPACE::currentObj<dummyptr>
			::current_value_t::lock lock(d->current_value);

		lock.wait([&lock, &uuid]
			  {
				  if (lock->value.null())
					  std::cerr << "null" << std::endl;
				  else
					  std::cerr << lock->value->s << " ("
						    << x::tostring(lock->value
								   ->uuid)
						    << ")" << std::endl;

				  return !lock->value.null() &&
					  lock->isinitial &&
					  lock->value->uuid == uuid &&
					  lock->value->s == "value3";
			  }
			  );
	}
	std::cerr << "Ok" << std::endl;
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		tstnodes nodes(3);

		x::property::load_property("objrepo::manager", "2 seconds",
					   true, true);

		test1(nodes);

	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

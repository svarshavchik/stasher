/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "stasher/versionedcurrent.H"
#include "stasher/current.H"
#include "stasher/puttransaction.H"
#include "stasher/versionedput.H"
#include "stasher/manager.H"
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

template class STASHER_NAMESPACE::versionedptr<dummyptr>;

typedef STASHER_NAMESPACE::currentObj<dummyptr,
				      STASHER_NAMESPACE::versionedptr<dummyptr>
				      > currentVersionedDummyObj;

typedef x::ref<currentVersionedDummyObj> currentVersionedDummy;

class completedObj : virtual public x::obj {

public:
	typedef x::mpcobj<STASHER_NAMESPACE::putresultsptr> flag_t;

	flag_t flag;

	completedObj()
	{
	}

	~completedObj()
	{
	}

	void completed(const STASHER_NAMESPACE::putresults &res)
	{
		flag_t::lock lock(flag);

		*lock=res;
		lock.notify_all();
	}

	bool done()
	{
		flag_t::lock lock(flag);

		return !lock->null();
	}

	STASHER_NAMESPACE::putresults wait()
	{
		flag_t::lock lock(flag);

		lock.wait( [&lock] { return !lock->null(); });
		return *lock;
	}

	void clear()
	{
		flag_t::lock lock(flag);

		*lock=STASHER_NAMESPACE::putresultsptr();
	}
};

typedef x::ref<completedObj> completed;

static void test1(tstnodes &t)
{
	x::destroy_callback::base::guard guard;

	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	auto client=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	guard(client);

	auto manager=STASHER_NAMESPACE::manager::create();

	auto mydummy=currentVersionedDummy::create();
	auto c=completed::create();

	guard(mydummy);

	auto c_completed_functor=[c](const STASHER_NAMESPACE::putresults &res)
		{
			c->completed(res);
		};

	auto subscription=mydummy->manage(manager, client, "fyle");

	{
		auto put=STASHER_NAMESPACE::client::base::transaction::create();

		put->newobj("fyle", "Fyle\n");

		decltype(mydummy->current_value)::lock
			lock(mydummy->current_value);

		STASHER_NAMESPACE::versioned_put_request_from(client, put,
							      c_completed_functor,
							      lock->value);
	}

	std::cerr << "Waiting for initial put to be processed"
		  << std::endl;
	auto res=c->wait();

	if (res->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION("Unexpected status: "
				<< x::to_string(res->status));

	std::cerr << "Waiting for the updated object"
		  << std::endl;

	{
		decltype(mydummy->current_value)::lock
			lock(mydummy->current_value);

		dummyptr testing_conversion(lock->value);

		testing_conversion=lock->value;

		lock.wait([&lock, &res]
			  {
				  return !lock->value.null() &&
					  lock->value->uuid == res->newuuid;
			  });
	}

	c->clear();

	{
		auto put=STASHER_NAMESPACE::client::base::transaction::create();

		put->newobj("fyle", "Fyle\n");

		decltype(mydummy->current_value)::lock
			lock(mydummy->current_value);

		STASHER_NAMESPACE::versioned_put_request_from(client, put,
							      c_completed_functor,
							      lock->value);
	}

	std::cerr << "Making sure that the code is waiting for an update"
		  << std::endl;
	sleep(2);

	if (c->done())
		std::cerr << "Transaction is not being held, for some reason"
			  << std::endl;

	std::cerr << "Looks like it is, kicking it" << std::endl;
	mydummy->update(dummyptr(), false);
	if (c->wait()->status != STASHER_NAMESPACE::req_rejected_stat)
		throw EXCEPTION("Unexpected status");


}

class test2Obj : virtual public x::obj {

public:

	class meta_t {

	public:
		dummyptr ptr;
		STASHER_NAMESPACE::req_stat_t stat;
		bool ptr_received;
		bool stat_received;

		meta_t() : ptr_received(false), stat_received(false)
		{
		}
	};

	typedef x::mpcobj<meta_t> status_t;

	status_t status;

	test2Obj() {}
	~test2Obj() {}
};

static void test2(tstnodes &t)
{
	x::destroy_callback::base::guard guard;

	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	auto client=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	guard(client);

	auto manager=STASHER_NAMESPACE::manager::create();

	auto t2=x::ref<test2Obj>::create();

	auto my_object=STASHER_NAMESPACE::make_versioned_current<dummyptr>
		( [t2]
		  (const dummyptr &newvalue, bool ignored)
		  {
			  test2Obj::status_t::lock lock(t2->status);

			  lock->ptr=std::move(newvalue);
			  lock->ptr_received=true;
			  lock.notify_all();
		  },
		  [t2]
		  (STASHER_NAMESPACE::req_stat_t status)
		  {
			  test2Obj::status_t::lock lock(t2->status);

			  lock->stat=status;
			  lock->stat_received=true;
			  lock.notify_all();
		  });

	auto mcguffin=my_object->manage(manager, client, "object");

	std::cout << "Waiting for the initial update" << std::endl;

	{
		test2Obj::status_t::lock lock(t2->status);

		lock.wait([&lock]
			  {
				  return lock->ptr_received &&
					  lock->stat_received;
			  });

		if (lock->stat != STASHER_NAMESPACE::req_processed_stat)
		{
			throw EXCEPTION("Unexpected status");
		}
	}

	{
		auto put=STASHER_NAMESPACE::client::base::transaction::create();

		put->newobj("object", "Object\n");

		client->put(put);
	}

	std::cout << "Waiting for the second update" << std::endl;

	{
		test2Obj::status_t::lock lock(t2->status);

		lock.wait([&lock]
			  {
				  return !lock->ptr.null() &&
					  lock->ptr->s == "Object";
			  });
	}

}

void test_versioned_put_request_fromiter(const STASHER_NAMESPACE::client &cl,
					 const STASHER_NAMESPACE::client
					 ::base::transaction &tr,
					 std::list<STASHER_NAMESPACE
					 ::versionedptr<dummyptr> >::iterator b,
					 std::list<STASHER_NAMESPACE
					 ::versionedptr<dummyptr> >::iterator e)
{
	versioned_put_request_fromiter(cl, tr,
				       []
				       (const STASHER_NAMESPACE::putresults
					&res)
				       {
				       },
				       b, e);
}

static void test3(tstnodes &t)
{
	x::destroy_callback::base::guard guard;

	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);

	auto client=STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	guard(client);

	auto manager=STASHER_NAMESPACE::manager::create();

	auto mydummy=currentVersionedDummy::create();

	std::cout << "Sending initial transaction" << std::endl;

	{
		auto put=STASHER_NAMESPACE::client::base::transaction::create();

		auto versions=STASHER_NAMESPACE::versionscollected::create();

		put->newobj("fyle2", "Fyle2\n");

		{
			decltype(mydummy->current_value)::lock
				lock(mydummy->current_value);

			versions->add(lock->value);
		}

		auto res=
			STASHER_NAMESPACE::versioned_put(client, put, versions);

		if (res->status != STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION("test3 failed: "
					+ x::to_string(res->status));

	}
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		tstnodes nodes(1);

		x::property::load_property("objrepo::manager", "2 seconds",
					   true, true);

		std::cout << "test1" << std::endl;
		test1(nodes);

		std::cout << "test2" << std::endl;
		test2(nodes);

		std::cout << "test3" << std::endl;
		test3(nodes);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

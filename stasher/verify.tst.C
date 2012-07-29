/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"

#include "stasher/client.H"
#include "stasher/manager.H"
#include "stasher/versionedptr.H"
#include "stasher/versionedput.H"
#include "stasher/current.H"
#include "stasher/versionedcurrent.H"
#include <x/ref.H>
#include <x/ptr.H>
#include <x/threads/run.H>
#include <x/dequemsgdispatcher.H>
#include <x/uuid.H>
#include <x/fd.H>
#include <x/destroycallbackflag.H>
#include <x/mpobj.H>

class dummyObj : virtual public x::obj {

public:

	x::uuid uuid;
	std::string name;

	dummyObj(const x::uuid &uuidArg) : uuid(uuidArg) {}

	dummyObj(const x::uuid &uuidArg, const x::fd &fdArg)
		: uuid(uuidArg)
	{
		std::getline(*fdArg->getistream(), name);
	}

	~dummyObj() noexcept
	{
	}
};

typedef STASHER_NAMESPACE::current<x::ptr<dummyObj>,
				   STASHER_NAMESPACE::versionedptr<
					   x::ptr<dummyObj> > > dummy;

class counterObj : virtual public x::obj {

public:

	x::mpcobj<int> counter;

	counterObj() : counter(0) {}
	~counterObj() noexcept {}
};

class keepitObj : public x::dequemsgdispatcherObj,
		  public x::runthreadsingleton {

	dummy *instance;
	const STASHER_NAMESPACE::manager *manager;
	const STASHER_NAMESPACE::client *client;

	std::string object;
	std::string name;

	bool update_pending;

public:

	x::ref<counterObj> counter;

	keepitObj(const std::string &objectArg,
		  const std::string &nameArg,
		  const x::ref<counterObj> &counterArg)
		: object(objectArg), name(nameArg), counter(counterArg)
	{
	}

	~keepitObj() noexcept
	{
	}

	void run(const STASHER_NAMESPACE::client &client);

#include "verify.tst.msgs.all.H"

};

void keepitObj::run(const STASHER_NAMESPACE::client &clientInstance)
{
	x::destroyCallbackFlag::base::guard guard;

	auto managerInstance=STASHER_NAMESPACE::manager::create();

	dummy dummyInstance=({

			x::ref<keepitObj> me(this);

			STASHER_NAMESPACE::make_versioned_current
				<x::ptr<dummyObj> >
				( [me]
				  (const x::ptr<dummyObj> &newValue, bool flag)
				  {
					  me->check();
				  }
				  );
		});

	guard(dummyInstance);

	manager= &managerInstance;
	client= &clientInstance;
	instance= &dummyInstance;
	update_pending=false;

	auto mcguffin = dummyInstance->manage(managerInstance,
					      clientInstance, object);

	try {
		while (1)
		{
			auto msg=({
					msgqueue_t::lock lock(msgqueue);

					lock.wait([&lock]
						  {
							  return !lock->empty();
						  });

					auto front=lock->front();
					lock->pop_front();

					front;
				});

			msg->dispatch();
		}
	} catch (const x::stopexception &e)
	{
	}
}

void keepitObj::dispatch(const check_msg &msg)
{
	if (update_pending)
		return;

	dummy::base::current_value_t::lock
		lock( (*instance)->current_value );

	if (!lock->value.null() && lock->value->name == name)
		return;

	std::cout << "Changing to: " << name << std::endl;

	auto tran=STASHER_NAMESPACE::client::base::transaction::create();

	if (lock->value.null())
	{
		tran->newobj(object, name + "\n");
	}
	else
	{
		tran->updobj(object, lock->value->uuid, name + "\n");
	}

	x::ref<keepitObj> me(this);

	STASHER_NAMESPACE::versioned_put_request_from
		(*client, tran,
		 [me]
		 (const STASHER_NAMESPACE::putresults &res)
		 {
			 me->check_put(res);
		 },
		 lock->value);

	update_pending=true;

}

void keepitObj::dispatch(const check_put_msg &msg)
{
	update_pending=false;

	std::cout << "Update: " << name << ": "
		  << x::tostring(msg.res->status) << std::endl;

	if (msg.res->status == STASHER_NAMESPACE::req_processed_stat)
	{
		x::mpcobj<int>::lock lock(counter->counter);

		++*lock;

		lock.notify_all();
	}

	dispatch(check_msg());
}

void test1(tstnodes &t)
{
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);
	t.startmastercontrolleron0_int(tnodes);
	t.startreconnecter(tnodes);

	STASHER_NAMESPACE::client cl0=
		STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(0));

	STASHER_NAMESPACE::client cl1=
		STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(1));

	STASHER_NAMESPACE::client cl2=
		STASHER_NAMESPACE::client::base::connect(tstnodes::getnodedir(2));

	auto counter=x::ref<counterObj>::create();

	auto firstone=x::ref<keepitObj>::create("object", "name1", counter),
		secondone=x::ref<keepitObj>::create("object", "name2", counter),
		thirdone=x::ref<keepitObj>::create("object", "name3", counter);

	auto thread1=x::run(firstone, cl0);
	auto thread2=x::run(secondone, cl1);
	auto thread3=x::run(thirdone, cl2);

	std::cout << "Waiting for 100 completed updates..." << std::endl;

	{
		x::mpcobj<int>::lock c_lock(counter->counter);

		while (!c_lock.wait_for(std::chrono::seconds(2),
					[&c_lock]
					{
						return *c_lock >= 100;
					}))
		{
			std::cout << *c_lock << std::endl;
		}
	}

	std::cout << "Stopping" << std::endl;

	firstone->stop();
	secondone->stop();
	thirdone->stop();

	thread1->get();
	thread2->get();
	thread3->get();
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		tstnodes nodes(3);
		test1(nodes);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

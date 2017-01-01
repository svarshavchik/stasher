/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"
#include "node.H"
#include "repocontrollermaster.H"
#include "stasher/client.H"
#include <x/options.H>
#include <x/destroy_callback.H>

#include <mutex>
#include <condition_variable>

static bool test_1_hook_enabled=false;
static bool test_1_hook_hooked;
static std::mutex test_1_mutex;
static std::condition_variable test_1_cv;

static bool test_2_hook_enabled=false;
static bool test_2_hook_hooked;
static std::mutex test_2_mutex;
static std::condition_variable test_2_cv;

#define DEBUG_MASTER_TEST_2_HOOK1() do {				\
		if (test_1_hook_enabled)				\
		{							\
			std::lock_guard<std::mutex> guard(test_1_mutex); \
									\
			test_1_hook_hooked=true;			\
			test_1_cv.notify_all();				\
		}							\
	} while (0)

#define DEBUG_MASTER_TEST_2_HOOK2() do {				\
		if (test_2_hook_enabled)				\
		{							\
			std::lock_guard<std::mutex> guard(test_2_mutex); \
									\
			test_2_hook_hooked=true;			\
			test_2_cv.notify_all();				\
		}							\
	} while (0)

#include "repocontrollermaster.C"

class sendstop : virtual public x::obj {

	LOG_CLASS_SCOPE;

public:
	sendstop() {}
	~sendstop() noexcept {}

	void run(const STASHER_NAMESPACE::client &start_arg)
	{
		try {
			start_arg->shutdown();
		} catch (const x::exception &e)
		{
			LOG_ERROR(e);
		}
	}
};

LOG_CLASS_INIT(sendstop);

void test1(tstnodes &t)
{
	test_1_hook_enabled=true;
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	t.startmastercontrolleron0(tnodes);

	tobjrepoObj::lockentryptr_t lock=({
			x::eventfd lock_event_fd=x::eventfd::create();

			std::set<std::string> s;

			s.insert("obj");

			auto lock=tnodes[0]->repo->lock(s, lock_event_fd);

			while (!lock->locked())
				lock_event_fd->event();

			lock;
		});

	STASHER_NAMESPACE::client
		cl=STASHER_NAMESPACE::client::base::admin(tstnodes::getnodedir(0));

	STASHER_NAMESPACE::client::base::transaction tran=
		STASHER_NAMESPACE::client::base::transaction::create();

	tran->newobj("obj", "obj_value\n");

	std::cout << "Putting" << std::endl;

	auto request=cl->put_request(tran);

	{
		std::unique_lock<std::mutex> lock(test_1_mutex);

		test_1_cv.wait(lock, [] { return test_1_hook_hooked; });
	}

	std::cout << "Hooked, stopping" << std::endl;
	test_1_hook_enabled=false;

	auto runthr=x::run(x::ref<sendstop>::create(), cl);
	std::cout << "Waiting" << std::endl;
	tnodes[0]->wait();
	runthr->wait();
}

void test2(tstnodes &t)
{
	test_2_hook_enabled=true;
	std::vector<tstnodes::noderef> tnodes;

	t.init(tnodes);

	t.startmastercontrolleron0(tnodes);

	tobjrepoObj::lockentryptr_t lock=({
			x::eventfd lock_event_fd=x::eventfd::create();

			std::set<std::string> s;

			s.insert("obj");

			auto lock=tnodes[1]->repo->lock(s, lock_event_fd);

			while (!lock->locked())
				lock_event_fd->event();

			lock;
		});

	STASHER_NAMESPACE::client
		cl=STASHER_NAMESPACE::client::base::admin(tstnodes::getnodedir(0));

	STASHER_NAMESPACE::client::base::transaction tran=
		STASHER_NAMESPACE::client::base::transaction::create();

	tran->newobj("obj", "obj_value\n");

	std::cout << "Putting" << std::endl;

	auto request=cl->put_request(tran);

	{
		std::unique_lock<std::mutex> lock(test_2_mutex);

		test_2_cv.wait(lock, [] { return test_2_hook_hooked; });
	}

	std::cout << "Hooked, stopping" << std::endl;
	test_2_hook_enabled=false;

	auto runthr=x::run(x::ref<sendstop>::create(), cl);
	std::cout << "Waiting" << std::endl;
	tnodes[0]->wait();
	runthr->wait();
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		{
			tstnodes nodes(2);

			std::cout << "test1" << std::endl;
			test1(nodes);
		}

		{
			tstnodes nodes(2);

			std::cout << "test2" << std::endl;
			test2(nodes);
		}

	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

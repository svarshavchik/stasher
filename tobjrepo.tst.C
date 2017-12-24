/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include <string>
#include <unistd.h>
#include <x/dir.H>
#include <x/options.H>

#include "objrepo_config.h"

static bool donotcancel=false;
static bool donotcommit=false;

#define CANCEL_DEBUG_HOOK(uuid) do {		\
	if (donotcancel) return;		\
	} while(0)

#define COMMIT_DEBUG_HOOK(uuid) do {		\
	if (donotcommit) return;		\
	} while(0)

#include "tobjrepo.C"
#include "objrepo.C"
#include "newtran.C"
#include "tran.C"
#include "trancommit.C"

static void test1()
{
	x::uuid tuuid;

	{
		tobjrepo repo(tobjrepo::create("conftestdir.tst"));

		newtran tran(repo->newtransaction());

		tran->newobj("obj1");

		tran=repo->newtransaction(); // [CREATING]
		tran->newobj("obj2");

		tuuid=tran->finalize();

		mkdir("conftestdir.tst/" DATA "/a", 0777);
		x::fd::base::open("conftestdir.tst/" DATA "/a/b.f", O_CREAT|O_RDWR,
			    0666);
	}

	{
		x::uuid ua;

		x::fd::base::open("conftestdir.tst/" TMP "/" +
			    x::tostring(ua), O_CREAT|O_RDWR, 0666);

		x::fd::base::open("conftestdir.tst/" TMP "/" +
			    x::tostring(ua) + ".1", O_CREAT|O_RDWR, 0666);
	}

	{
		tobjrepo repo(tobjrepo::create("conftestdir.tst"));

		x::dir tmpdir=x::dir::create("conftestdir.tst/" TMP);

		std::set<std::string> tfiles;

		tfiles.insert(tmpdir->begin(), tmpdir->end());
		tfiles.insert(repo->obj_begin(), repo->obj_end());

		if (tfiles.size() != 2 ||
		    tfiles.find( x::tostring(tuuid) + "." T_SUFFIX) ==
		    tfiles.end() ||
		    tfiles.find( x::tostring(tuuid) + ".0") ==
		    tfiles.end())
			throw EXCEPTION("Transaction temp file sanity check failed"); // [CLEANUP]

	}

	x::dir::base::rmrf("conftestdir.tst");

	{
		tobjrepo repo(tobjrepo::create("conftestdir.tst"));

		newtran tran(repo->newtransaction());

		tran->newobj("obj1");

		tuuid=tran->finalize();

		donotcancel=true;
		repo->cancel(tuuid);	// CANCEL
		donotcancel=false;

		std::set<std::string> tmpfiles;

		x::dir tmpdir=x::dir::create("conftestdir.tst/" TMP);

		std::set<std::string> tfiles;

		tfiles.insert(tmpdir->begin(), tmpdir->end());

		if (tfiles.size() != 2 ||
		    tfiles.find( x::tostring(tuuid) + "." X_SUFFIX) ==
		    tfiles.end() ||
		    tfiles.find( x::tostring(tuuid) + ".0") ==
		    tfiles.end())
			throw EXCEPTION("Transaction temp file sanity check #2 failed"); // [FILES]
	}

	{
		tobjrepo repo(tobjrepo::create("conftestdir.tst")); // [CLEANUP]

		std::set<std::string> tmpfiles;

		x::dir tmpdir=x::dir::create("conftestdir.tst/" TMP);

		std::set<std::string> tfiles;

		tfiles.insert(tmpdir->begin(), tmpdir->end());

		if (!tfiles.empty())
			throw EXCEPTION("Transaction temp file sanity check #3 failed"); // [FILES]
	}
}

static void test2()
{
	x::dir::base::rmrf("conftestdir.tst");

	x::uuid firstuuid, seconduuid;

	{
		tobjrepo repo(tobjrepo::create("conftestdir.tst"));

		{
			newtran tran(repo->newtransaction());

			tran->newobj("obj1");
			tran->newobj("obj2");

			firstuuid=tran->finalize();
		}

		donotcommit=true;

		repo->begin_commit(firstuuid,
				   x::eventfd::create())->commit();
		donotcommit=false;
	}

	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	repo->cancel(firstuuid);

	tobjrepoObj::values_t valuesMap;
	std::set<std::string> notfound;
	{
		std::set<std::string> s;

		s.insert("obj1");
		s.insert("obj2");

		repo->values(s, false, valuesMap, notfound);
	}

	if (!notfound.empty() ||
	    valuesMap.find("obj1") == valuesMap.end() ||
	    valuesMap.find("obj2") == valuesMap.end()) // [CLEANUP]
		throw EXCEPTION("Transaction recommit failed");

	{
		newtran tran(repo->newtransaction());

		tran->newobj("obj1");

		x::uuid tu(tran->finalize());

		trancommit t(repo->begin_commit(tu, x::eventfd::create()));

		if (t->verify())
			throw EXCEPTION("VERIFY (part 1) failed");
		repo->cancel(tu);
	}

	{
		newtran tran(repo->newtransaction());

		tran->delobj("obj3", firstuuid);
		tran->newobj("obj4");

		x::uuid tu(tran->finalize());

		trancommit t(repo->begin_commit(tran->finalize(),
						   x::eventfd::create()));

		if (t->verify())
			throw EXCEPTION("VERIFY (part 2) failed");
		repo->cancel(tu);
	}

	{
		newtran tran(repo->newtransaction());

		tran->delobj("obj1", seconduuid);

		x::uuid tu(tran->finalize());

		trancommit t(repo->begin_commit(tu, x::eventfd::create()));

		if (t->verify())
			throw EXCEPTION("VERIFY (part 3) failed");
		repo->cancel(tu);
	}

	{
		newtran tran(repo->newtransaction());

		tran->updobj("obj1", seconduuid);

		x::uuid tu(tran->finalize());

		trancommit t(repo->begin_commit(tu, x::eventfd::create()));

		if (t->verify())
			throw EXCEPTION("VERIFY (part 4) failed");

		repo->cancel(tu);
	}

	{
		newtran tran(repo->newtransaction());

		tran->delobj("obj1", firstuuid);
		tran->updobj("obj2", firstuuid);
		tran->newobj("obj3");

		x::uuid tu(tran->finalize());

		trancommit t(repo->begin_commit(tu, x::eventfd::create()));

		if (!t->verify())
			throw EXCEPTION("VERIFY (part 5) failed");

		t->commit(); // [TRANCOMMIT]
		repo->cancel(tu);
	}

	valuesMap.clear();
	notfound.clear();

	{
		std::set<std::string> s;

		s.insert("obj1");
		s.insert("obj2");
		s.insert("obj3");
		repo->values(s, false, valuesMap, notfound);
	}

	if (notfound.size() != 1 ||
	    *notfound.begin() != "obj1" ||
	    valuesMap.find("obj1") != valuesMap.end() ||
	    valuesMap.find("obj2") == valuesMap.end() ||
	    valuesMap.find("obj3") == valuesMap.end()) // [COMMIT]
		throw EXCEPTION("Transaction commit failed");

	{
		std::set<std::string> s;

		x::dir tmpdir=x::dir::create("conftestdir.tst/" TMP);

		s.insert(tmpdir->begin(), tmpdir->end());

		if (!s.empty())
			throw EXCEPTION("Transaction commit had something left over in tmp");
	}
}

static void test3()
{
	x::dir::base::rmrf("conftestdir.tst");

	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	x::uuid firstuuid, seconduuid;

	{
		newtran tran(repo->newtransaction());

		tran->newobj("obj1");

		firstuuid=tran->finalize();
	}

	{
		newtran tran(repo->newtransaction());

		tran->newobj("obj1");

		seconduuid=tran->finalize();
	}

	trancommit tr1(repo->begin_commit(firstuuid, x::eventfd::create()));

	if (!tr1->ready())
		throw EXCEPTION("First transaction not ready?");

	trancommit tr2(repo->begin_commit(seconduuid, x::eventfd::create()));

	if (tr2->ready()) // [READY]
		throw EXCEPTION("Second transaction is ready?");

	tr1->commit();

	if (!tr2->ready())
		throw EXCEPTION("Second transaction not ready?");
}

static void test4() // [GLOBALLOCK]
{
	x::eventfd fd(x::eventfd::create());

	fd->nonblock(true);

	struct pollfd pfd=pollfd();

	pfd.fd=fd->get_fd();
	pfd.events=POLLIN;

	x::dir::base::rmrf("conftestdir.tst");

	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	x::uuid firstuuid;

	{
		newtran tran(repo->newtransaction());

		tran->newobj("obj1");

		firstuuid=tran->finalize();
	}

	trancommitptr tr(repo->begin_commit(firstuuid, fd));

	if (!tr->ready())
		throw EXCEPTION("test4: Transaction is not ready for some reason");

	tobjrepoObj::commitlockptr_t commitlock(repo->commitlock(fd));

	if (commitlock->locked())
		throw EXCEPTION("test4: commit lock is ready for some reason");

	while (poll(&pfd, 1, 0) > 0)
		fd->event();

	repo->cancel(firstuuid);
	tr=trancommitptr();

	do
	{
		if (poll(&pfd, 1, -1) < 0)
			throw SYSEXCEPTION("poll");

		if (pfd.revents & POLLIN)
			fd->event();

	} while (!commitlock->locked());

	{
		newtran tran(repo->newtransaction());

		tran->newobj("obj1");

		firstuuid=tran->finalize();
	}

	tr=repo->begin_commit(firstuuid, fd);

	if (tr->ready())
		throw EXCEPTION("Test4: Transaction is ready for some reason");

	while (poll(&pfd, 1, 0) > 0)
		fd->event();

	commitlock=tobjrepoObj::commitlockptr_t();

	do
	{
		if (poll(&pfd, 1, -1) < 0)
			throw SYSEXCEPTION("poll");

		if (pfd.revents & POLLIN)
			fd->event();

	} while (!tr->ready());
}

static void test5()
{
	x::eventfd fd(x::eventfd::create());

	fd->nonblock(true);

	struct pollfd pfd=pollfd();

	pfd.fd=fd->get_fd();
	pfd.events=POLLIN;

	x::dir::base::rmrf("conftestdir.tst");

	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	x::uuid firstuuid;

	{
		newtran tran(repo->newtransaction());

		x::fd fd(tran->newobj("obj1"));

		{
			x::ostream o(fd->getostream());

			(*o) << "foobar" << std::endl << std::flush;
		}
		fd->close();

		firstuuid=tran->finalize();
	}

	trancommit tr(repo->begin_commit(firstuuid, fd));

	if (!tr->ready())
		throw EXCEPTION("test5: initial transaction not ready for some reason");

	std::set<std::string> objset;

	objset.insert("obj1");
	objset.insert("obj2");

	tobjrepoObj::lockentry_t lock(repo->lock(objset, fd));

	while (poll(&pfd, 1, 0) > 0)
		fd->event();

	if (lock->locked())
		throw EXCEPTION("test5: lock is already ready for some reason");

	tr->commit();

	while (poll(&pfd, 1, 0) > 0)
	{
		fd->event();
		if (lock->locked()) // [OBJECTLOCK]
			break;
	}

	tobjrepoObj::values_t valuesMap;
	std::set<std::string> notfound;

	repo->values(objset, false, valuesMap, notfound); // [VALUES]

	if (notfound.size() != 1 ||
	    *notfound.begin() != "obj2" ||
	    valuesMap.size() != 1 ||
	    valuesMap.begin()->first != "obj1" ||
	    valuesMap.begin()->second.first != firstuuid)
		throw EXCEPTION("test5: [VALUES] failed (false)");

	valuesMap.clear();
	notfound.clear();

	repo->values(objset, true, valuesMap, notfound); // [VALUES]

	if (notfound.size() == 1 &&
	    *notfound.begin() == "obj2" &&
	    valuesMap.size() == 1 &&
	    valuesMap.begin()->first == "obj1" &&
	    valuesMap.begin()->second.first == firstuuid)
	{
		x::istream i(valuesMap.begin()->second.second->getistream());

		std::string l;

		std::getline(*i, l);

		if (l == "foobar")
			return;
	}

	throw EXCEPTION("test5: [VALUES] failed (true)");
}

static void test6()
{
	x::dir::base::rmrf("conftestdir.tst");

	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	x::uuid firstuuid;

	{
		newtran tr(repo->newtransaction(firstuuid));

		tr->getOptions()[tobjrepoObj::node_opt]="nodea";

		(*tr->newobj("foo/bar")->getostream())
			<< "foobar" << std::flush;

		repo->begin_commit(tr->finalize(), x::eventfd::create())
			->commit();
	}

	std::set<std::string> objset;

	objset.insert(std::string(tobjrepoObj::done_hier) + "/nodea/" +
		      x::tostring(firstuuid)); // [MARKDONE]

	{
		tobjrepoObj::values_t valuesMap;
		std::set<std::string> notfound;

		repo->values(objset, true, valuesMap, notfound);

		std::string s;

		std::getline(*valuesMap[*objset.begin()].second->getistream(),
			     s);

		if (s != "0")
			throw EXCEPTION("[MARKDONE] failed");
	}

	objset.clear();

	std::copy(repo->obj_begin(tobjrepoObj::done_hier), repo->obj_end(),
		  std::insert_iterator<std::set<std::string> >
		  (objset, objset.end()));

	if (objset.size() != 1 ||
	    *objset.begin() != std::string(tobjrepoObj::done_hier) + "/nodea/" +
	    x::tostring(firstuuid))
		throw EXCEPTION("[OBJITER] failed");

	repo->cancel(firstuuid);
	objset.clear();
	std::copy(repo->obj_begin(), repo->obj_end(),
		  std::insert_iterator<std::set<std::string> >
		  (objset, objset.end()));

	if (objset.size() != 1 || *objset.begin() != "foo/bar")
		throw EXCEPTION("[CANCELMARKDONE] failed");
}

static void test7()
{
	x::dir::base::rmrf("conftestdir.tst");

	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	x::uuid tran1=repo->newtransaction()->finalize();

	repo->mark_done(tran1, "node1", STASHER_NAMESPACE::req_rejected_stat,
			x::ptr<x::obj>());

	//! [MARKDONESTAT]
	if (repo->get_tran_stat("node1", tran1)
	    != STASHER_NAMESPACE::req_rejected_stat ||
	    repo->get_tran_stat("node2", tran1)
	    != STASHER_NAMESPACE::req_failed_stat ||
	    repo->get_tran_stat("node1", x::uuid())
	    != STASHER_NAMESPACE::req_failed_stat)
		throw EXCEPTION("[MARKDONESTAT] failed [1 of 3]");

	repo->cancel_done("node2");

	if (repo->get_tran_stat("node1", tran1)
	    != STASHER_NAMESPACE::req_rejected_stat ||
	    repo->get_tran_stat("node2", tran1)
	    != STASHER_NAMESPACE::req_failed_stat ||
	    repo->get_tran_stat("node1", x::uuid())
	    != STASHER_NAMESPACE::req_failed_stat)
		throw EXCEPTION("[MARKDONESTAT] failed [2 of 3]");

	repo->cancel_done("node1");

	if (repo->get_tran_stat("node1", tran1)
	    != STASHER_NAMESPACE::req_failed_stat)
		throw EXCEPTION("[MARKDONESTAT] failed [3 of 3]");
}

class test8_cb : public tobjrepoObj::finalized_cb {

public:
	test8_cb() noexcept {}
	~test8_cb() {}

	std::map<x::uuid, dist_received_status_t> m;

	void operator()(const x::uuid &uuid,
			const dist_received_status_t &status)

	{
		m[uuid]=status;
	}
};

void test8()
{
	x::dir::base::rmrf("conftestdir.tst");

	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	x::uuid tran1;

	{
		newtran tr=repo->newtransaction(tran1);

		tr->getOptions()[tobjrepoObj::node_opt]="a";

		tr->finalize();
	}

	x::uuid tran2;

	repo->failedlist_insert(tran2, dist_received_status_t
				(dist_received_status_err, "b"));

	test8_cb cb;
	repo->enumerate(cb);

	if (cb.m.size() != 2 ||
	    cb.m[tran1].sourcenode != "a" ||
	    cb.m[tran1].status != dist_received_status_ok ||
	    cb.m[tran2].sourcenode != "b" ||
	    cb.m[tran2].status != dist_received_status_err) // [TRANENUMERATE]
		throw EXCEPTION("test8 failed (1)");

	repo->mark_done(tran2, "a", STASHER_NAMESPACE::req_failed_stat,
			x::ptr<x::obj>());

	cb.m.clear();
	repo->enumerate(cb);

	if (cb.m.size() != 2 ||
	    cb.m[tran1].sourcenode != "a" ||
	    cb.m[tran1].status != dist_received_status_ok ||
	    cb.m[tran2].sourcenode != "b" ||
	    cb.m[tran2].status != dist_received_status_err)
		throw EXCEPTION("test8 failed (2)"); // [FAILMARKDONE]

	repo->cancel(tran2);

	cb.m.clear();
	repo->enumerate(cb);

	if (cb.m.size() != 1 ||
	    cb.m[tran1].sourcenode != "a" ||
	    cb.m[tran1].status != dist_received_status_ok)
		throw EXCEPTION("test8 failed (3)"); // [FAILCANCEL]

}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	ALARM(30);
	try {
		x::dir::base::rmrf("conftestdir.tst");
		test1();
		test2();
		test3();
		test4();
		test5();
		test6();
		test7();
		test8();
		x::dir::base::rmrf("conftestdir.tst");
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

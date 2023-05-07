/*
** Copyright 2012-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include <string>
#include <unistd.h>

static void (*mkdiring_debug_hook)(std::string);
static void (*mkdired_debug_hook)(std::string);
static void (*rmdiring_debug_hook)(std::string);
static void (*rmdired_debug_hook)(std::string);

#define MKDIRING_DEBUG_HOOK(s) if (mkdiring_debug_hook) mkdiring_debug_hook(s)
#define MKDIRED_DEBUG_HOOK(s) if (mkdired_debug_hook) mkdired_debug_hook(s)
#define RMDIRING_DEBUG_HOOK(s) if (rmdiring_debug_hook) rmdiring_debug_hook(s)
#define RMDIRED_DEBUG_HOOK(s) if (rmdired_debug_hook) rmdired_debug_hook(s)

#include "objrepo.C"

#include <x/options.H>
#include <cstdlib>
#include <iostream>
#include <iterator>

#include <sys/types.h>
#include <sys/wait.h>

static void test_instance()
{
	std::filesystem::remove_all("conftest.dir");

	objrepoptr repo1(objrepo::create("conftest.dir")); // [CREATE]

	pid_t p=fork();

	if (p < 0)
		throw SYSEXCEPTION("fork");

	if (p == 0)
	{
		repo1=objrepoptr();

		try {
			repo1=objrepo::create("conftest.dir");
		} catch (const x::exception &e)
		{
			exit(0); //[LOCK]
		}
		exit(1);
	}

	int exitstat;

	if (wait(&exitstat) != p)
		throw EXCEPTION("Unexpected exit status from wait()");

	if (!WIFEXITED(exitstat) || WEXITSTATUS(exitstat) != 0)
		throw EXCEPTION("Failed");
}

static void test_tmp()
{
	objrepo repo1(objrepo::create("conftest.dir"));
	int counter=0;

	repo1->tmp_open("bar", O_RDWR|O_CREAT);

	repo1->tmp_rename("bar", "foo"); // [TMPRENAME]

	repo1->tmp_get("foo")->stat(); // [TMPGET]

	repo1->tmp_link("foo", "foobar"); // [TMPLINK]

	try {
		repo1->tmp_open("", O_RDONLY); // [TMPVALID]
	} catch (const x::exception &e)
	{
		++counter;
	}

	try {
		repo1->tmp_open(".foo", O_WRONLY|O_CREAT); // [TMPVALID]
	} catch (const x::exception &e)
	{
		++counter;
	}

	try {
		repo1->tmp_open("foo/", O_WRONLY|O_CREAT);
	} catch (const x::exception &e)
	{
		++counter;
	}

	if (counter != 3)
		throw EXCEPTION("Allowed a bad temporary filename");

	std::set<std::string> tmpnames;

	for (auto [first, second] = repo1->tmp_iter(); first != second; ++first)
	{
		tmpnames.insert(first->path().filename());
	}

	if (tmpnames.size() != 2 ||
	    tmpnames.find("foo") == tmpnames.end() ||
	    tmpnames.find("foobar") == tmpnames.end())
		throw EXCEPTION("Temporary filename iterator failed");
	// [TMPITER]
}

static objrepoObj *p;

static void trigger_after_a(std::string s)
{
	if (s == "a")
	{
		mkdired_debug_hook=0;

		p->tmp_open("obj2_tmp", O_RDWR|O_CREAT);
		p->tmp_get("obj2_tmp")->setattr(XATTRSERIAL,
						x::to_string(x::uuid()));
		p->obj_install("obj2_tmp", "a/b/c/obj2",
			       x::ptr<x::obj>());
	}
}

static void test_obj1()
{
	objrepo repo1(objrepo::create("conftest.dir"));

	p=&*repo1;

	mkdired_debug_hook=trigger_after_a;

	repo1->tmp_open("obj1_tmp", O_RDWR|O_CREAT);

	repo1->tmp_get("obj1_tmp")->stat(); // [TMPGET]

	x::uuid obj1_uuid;

	repo1->tmp_get("obj1_tmp")->setattr(XATTRSERIAL,
					    x::to_string(obj1_uuid));

	repo1->obj_install("obj1_tmp", "a/b/c/obj.1",
			   x::ptr<x::obj>()); // [OBJINSTALL]

	repo1->obj_get("a/b/c/obj.1")->stat();
	repo1->obj_get("a/b/c/obj2")->stat();

	if (repo1->obj_open("a/b/c/obj.1").null())
		throw EXCEPTION("obj_open() failed"); // [OPEN]

	{
		std::set<std::string> n;

		n.insert("a/b/c/obj.1");
		n.insert("a/b/c/obj.2");

		objrepoObj::values_t values;
		std::set<std::string> notfound;

		repo1->values(n, true, values, notfound);

		if (values.size() != 1 ||
		    values.begin()->second.first != obj1_uuid ||
		    values.begin()->second.second.null() ||
		    notfound.size() != 1 ||
		    *notfound.begin() != "a/b/c/obj.2")
			throw EXCEPTION("Unexpected result from values() (1)");

		values.clear();
		notfound.clear();

		repo1->values(n, false, values, notfound);

		if (values.size() != 1 ||
		    values.begin()->second.first != obj1_uuid ||
		    notfound.size() != 1 ||
		    *notfound.begin() != "a/b/c/obj.2")
			throw EXCEPTION("Unexpected result from values() (1)");
	}

	repo1->obj_open("a/b/c/obj.2");

	std::set<std::string> s;

	s.insert(repo1->obj_begin(), repo1->obj_end());

	if (s.size() != 2 ||
	    s.find("a/b/c/obj.1") == s.end() ||
	    s.find("a/b/c/obj2") == s.end())	// [OBJITER]
	{
		throw EXCEPTION("Object iteration failed");
	}

}

static void trigger_before_c(std::string s)
{
	if (s == "a/b/c")
	{
		rmdiring_debug_hook=0;

		p->obj_remove("a/b/c/obj2",
			      x::ptr<x::obj>());
	}
}

static void test_obj2()
{
	objrepo repo1(objrepo::create("conftest.dir"));

	p=&*repo1;

	if (!repo1->obj_exists("a/b/c/obj.1") ||
	    !repo1->obj_exists("a/b/c/obj2"))
		throw EXCEPTION("Cannot find existing objects");

	mkdired_debug_hook=trigger_after_a;

	repo1->tmp_open("obj3_tmp", O_RDWR|O_CREAT);
	repo1->obj_install("obj3_tmp", "a/b/c/obj\\3",
			   x::ptr<x::obj>());

	repo1->obj_get("a/b/c/obj\\3")->stat(); // [OBJGET]

	repo1->obj_remove("a/b/c/obj\\3",
			  x::ptr<x::obj>()); // [OBJREMOVE]

	rmdiring_debug_hook=trigger_before_c;

	repo1->obj_remove("a/b/c/obj.1",
			  x::ptr<x::obj>());

	if (repo1->obj_exists("a/b/c/obj.1") ||	// [OBJEXISTS]
	    repo1->obj_exists("a/b/c/obj2") ||
	    repo1->obj_exists("a/b/c/obj\\3") ||
	    access("conftest.dir/" DATA, 0) ||
	    access("conftest.dir/" DATA "/a", 0) == 0)	// [SUBDIRS]
		throw EXCEPTION("Objects weren't removed");
}

static void trigger_after_ab(std::string s)
{
	if (s == "a/b")
	{
		mkdired_debug_hook=0;
		p->tmp_open("obj2_tmp", O_RDWR|O_CREAT);
		p->obj_install("obj2_tmp", "a/b/c/obj2",
			       x::ptr<x::obj>());
		p->obj_remove("a/b/c/obj2",
			      x::ptr<x::obj>());
	}
}

static void test_obj3()
{
	objrepo repo1(objrepo::create("conftest.dir"));

	p=&*repo1;

	mkdired_debug_hook=trigger_after_ab;

	repo1->tmp_open("obj1_tmp", O_RDWR|O_CREAT);
	repo1->obj_install("obj1_tmp", "a/b/c/obj1",
			   x::ptr<x::obj>());

	if (!repo1->obj_exists("a/b/c/obj1"))
		throw EXCEPTION("Object wasn't installed");

}

static void test_obj4()
{
	std::filesystem::remove_all("conftest.dir");

	{
		objrepo repo1(objrepo::create("conftest.dir"));
	}

	mkdir("conftest.dir/" DATA "/a", 0700);
	x::fd::base::open("conftest.dir/" DATA "/a/b", O_CREAT|O_RDWR, 0666);
	x::fd::base::open("conftest.dir/" TMP "/.foo", O_CREAT|O_RDWR, 0666);

	if (access("conftest.dir/" DATA "/a/b", 0))
		throw EXCEPTION("Could not create junk data file");

	if (access("conftest.dir/" TMP "/.foo", 0))
		throw EXCEPTION("Could not create junk tmp file");

	{
		objrepo repo1(objrepo::create("conftest.dir"));
	}

	if (access("conftest.dir/" DATA "/a", 0) == 0 || // [VALID]
	    access("conftest.dir/" TMP "/.foo", 0) == 0 ||
	    access("conftest.dir/" DATA, 0))
		throw EXCEPTION("Validation failed");
}

class notifierObj : public objrepoObj::notifierObj {

public:
	std::set<std::string> installed_set, removed_set;

	notifierObj()
	{
	}

	~notifierObj()
	{
	}

	void installed(const std::string &objname,
		       const x::ptr<x::obj> &lock) override
	{
		if (!p->obj_exists(objname))
			throw EXCEPTION("installed callback failed");

		installed_set.insert(objname);
	}

	void removed(const std::string &objname,
		     const x::ptr<x::obj> &lock) override
	{
		if (p->obj_exists(objname))
			throw EXCEPTION("removed callback failed");

		removed_set.insert(objname);
	}
};

static void test_obj5()
{
	std::filesystem::remove_all("conftest.dir");

	objrepo repo1(objrepo::create("conftest.dir"));

	p=&*repo1;

	auto n=x::ref<notifierObj>::create();

	repo1->installNotifier(n);

	repo1->tmp_open("obj1_tmp", O_RDWR|O_CREAT);
	repo1->obj_install("obj1_tmp", "a/obj1",
			   x::ptr<x::obj>());
	repo1->obj_remove("a/obj1",
			  x::ptr<x::obj>());

	if (n->installed_set.size() != 1 ||
	    n->removed_set.size() != 1 ||
	    n->installed_set.find("a/obj1") == n->installed_set.end() ||
	    n->removed_set.find("a/obj1") == n->installed_set.end())
		throw EXCEPTION("Notification test failed");
	// [NOTIFIERS]
}

static void test_obj6()
{
	std::filesystem::remove_all("conftest.dir");

	objrepo repo(objrepo::create("conftest.dir"));

	repo->tmp_open("x", O_RDWR|O_CREAT);
	repo->obj_install("x", "a/obj.1", x::ptr<x::obj>());

	repo->tmp_open("x", O_RDWR|O_CREAT);
	repo->obj_install("x", "a/obj.1/a", x::ptr<x::obj>());

	repo->tmp_open("x", O_RDWR|O_CREAT);
	repo->obj_install("x", "a/b", x::ptr<x::obj>());

	std::set<std::string> s;

	s.insert(repo->dir_begin("a"), repo->dir_end());

	if (s.size() != 3 ||
	    s.find("a/b") == s.end() ||
	    s.find("a/obj.1") == s.end() ||
	    s.find("a/obj.1/") == s.end())
		throw EXCEPTION("[OBJDIRITER] failed (1)");

	s.clear();
	s.insert(repo->dir_begin("a/obj.1"), repo->dir_end());

	if (s.size() != 1 ||
	    s.find("a/obj.1/a") == s.end())
		throw EXCEPTION("[OBJDIRITER] failed (2)");

	s.clear();
	s.insert(repo->dir_begin(), repo->dir_end());

	if (s.size() != 1 ||
	    s.find("a/") == s.end())
		throw EXCEPTION("[OBJDIRITER] failed (3)");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	ALARM(30);
	try {
		std::cout << "test: instance" << std::endl;
		test_instance();

		std::cout << "test: tmp" << std::endl;
		test_tmp();

		std::cout << "test: obj(1)" << std::endl;
		test_obj1();

		std::cout << "test: obj(2)" << std::endl;
		test_obj2();

		std::cout << "test: obj(3)" << std::endl;
		test_obj3();

		std::cout << "test: obj(4) (expected errors follows)"
			  << std::endl;
		test_obj4();

		std::cout << "test: obj(5)" << std::endl;
		test_obj5();

		std::cout << "test: obj(6)" << std::endl;
		test_obj6();
		std::filesystem::remove_all("conftest.dir");
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

/*
** Copyright 2012-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "objrepocopysrc.H"
#include "objrepocopydst.H"
#include "newtran.H"
#include "trancommit.H"
#include "objserializer.H"
#include "objuuidenumerator.H"
#include "objuuidlist.H"
#include "baton.H"
#include <sstream>
#include <iostream>
#include <x/serialize.H>
#include <x/deserialize.H>
#include <x/destroycallbackflagobj.H>
#include <x/options.H>

static x::uuid mkobj(const tobjrepo &repo,
		     const std::string &objname)
{
	newtran tran(repo->newtransaction());

	tran->newobj(objname);

	x::uuid uuid(tran->finalize());

	repo->begin_commit(uuid, x::eventfd::create())->commit();

	return uuid;
}

class simdst : public objrepocopydstinterfaceObj {

public:

	tobjrepo repo;

	objrepocopydstinterfaceptr realdst;

	simdst(const tobjrepo &repoArg) : repo(repoArg)
	{
	}

	~simdst() noexcept
	{
	}

	void event(const objrepocopy::batonrequest &msg)

	{
		realdst->event(msg);
	}

	void event(const objrepocopy::masterlist &msg) // [SENDMASTERLIST]

	{
		realdst->event(msg);
	}

	void event(const objrepocopy::masterlistdone &msg) // [SENDMASTERLIST]

	{
		realdst->event(msg);
	}

	void event(const objrepocopy::slaveliststart &msg) // [SLAVELISTSTART]

	{
		realdst->event(msg);
	}

	void event(const objrepocopy::masterack &msg) // [SENDMASTERACK]

	{
		realdst->event(msg);
	}

	void event(const objrepocopy::copycomplete &msg) // [SENDCOPYCOMPLETE]

	{
		realdst->event(msg);
	}

	void event(const objserializer &msg)

	{
		// Serialize this into a temporary buffer

		std::vector<char> buffer;

		{
			typedef std::back_insert_iterator< std::vector<char >
							   > ins_iter_t;

			ins_iter_t ins_iter(buffer);

			typedef x::serialize
				::iterator<ins_iter_t> ser_iter_t;

			ser_iter_t ser_iter(ins_iter);

			ser_iter(msg);
		}

		// Now, deserialize the buffer using an objserializer for
		// the destination repo.

		objserializer deser(repo, "dummy");

		{
			typedef std::vector<char>::const_iterator iter_t;

			iter_t b(buffer.begin()), e(buffer.end());

			typedef x::deserialize
				::iterator<iter_t> deser_iter_t;

			deser_iter_t deser_iter(b, e);

			deser_iter(deser);
		}
	}
};

class simsrc : public objrepocopysrcinterfaceObj {

public:

	tobjrepo repo;

	objrepocopysrcinterfaceptr realsrc;

	simsrc(const tobjrepo &repoArg)
		: repo(repoArg)
	{
	}

	~simsrc() noexcept
	{
	}

	void event(const objrepocopy::batonresponse &msg)

	{
		realsrc->event(msg);
	}

	void event(const objrepocopy::slavelist &msg)

	{
		realsrc->event(msg);
	}

	void event(const objrepocopy::slavelistready &msg)

	{
		realsrc->event(msg);
	}

	void event(const objrepocopy::slavelistdone &msg)

	{
		realsrc->event(msg);
	}
};


static void test1()
{
	tobjrepo repo(tobjrepo::create("conftestdir.tst"));

	std::map<std::string, x::uuid> uuidmap, uuidmap2;

	for (size_t i=objuuidlistObj::default_chunksize.getValue()*2; i; --i)
	{
		std::stringstream ss;

		ss << "obj" << i;

		std::string name(ss.str());

		uuidmap[name]=mkobj(repo, name);
	}

	tobjrepo repo2(tobjrepo::create("conftestdir2.tst"));

	mkobj(repo2, "objx");

	x::ptr<simsrc> src(x::ptr<simsrc>::create(repo));
	x::ptr<simdst> dst(x::ptr<simdst>::create(repo2));

	objrepocopysrc realsrc(objrepocopysrc::create());

	objrepocopydst realdst(objrepocopydst::create());

	src->realsrc=realsrc;
	dst->realdst=realdst;

	boolref flag(boolref::create());

	realdst->start(repo2, src, flag, batonptr(),
		       x::ptr<x::obj>::create());

	x::ptr<x::obj> mcguffin(x::ptr<x::obj>::create());

	objrepocopysrcObj::copycomplete complete=
		realsrc->start(repo, dst, batonptr(), mcguffin);

	{
		auto cb=x::ref<x::destroyCallbackFlagObj>::create();

		mcguffin->ondestroy([cb]{cb->destroyed();});
		mcguffin=x::ptr<x::obj>();
		std::cout << "Testing [COPYSRCMCGUFFIN]" << std::endl;
		cb->wait(); // [COPYSRCMCGUFFIN]
	}

	complete->release();

	realsrc->wait();
	realdst->wait();

	if (!flag->flag)
		throw EXCEPTION("flag wasn't set to true");

	auto getrepo2=x::ref<objuuidenumeratorObj>::create(repo2);
	x::ptr<objuuidlistObj> objects;

	while (!(objects=getrepo2->next()).null())
	{
		uuidmap2.insert(objects->objuuids.begin(),
				objects->objuuids.end());
	}

	if (uuidmap != uuidmap2) // [IDENTICAL]
		throw EXCEPTION("Repository copy did not work");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	ALARM(30);

	try {
		x::dir::base::rmrf("conftestdir.tst");
		x::dir::base::rmrf("conftestdir2.tst");

		std::cerr << "test1" << std::endl;
		test1();
		x::dir::base::rmrf("conftestdir.tst");
		x::dir::base::rmrf("conftestdir2.tst");
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

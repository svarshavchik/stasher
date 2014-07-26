/*
** Copyright 2012-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepocopysrc.H"
#include "objrepocopydstinterface.H"
#include "objserializer.H"
#include "trancommit.H"
#include "baton.H"
#include "objuuidenumerator.H"
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

static void delobj(const tobjrepo &repo,
		   const std::string &objname)
{
	newtran tran(repo->newtransaction());

	tran->delobj(objname, x::uuid());

	repo->begin_commit(tran->finalize(), x::eventfd::create())->commit();
}

class simdst : public objrepocopydstinterfaceObj {

public:

	tobjrepo repo;
	x::weakptr<objrepocopysrcinterfaceptr> src;

	x::ptr<objuuidenumeratorObj> e;

	bool complete;
	std::mutex mutex;
	std::condition_variable cond;

	bool inprogress;

	x::ptr<simdst> *nukemyself;

	simdst(const tobjrepo &repoArg)
 : repo(repoArg),
				      complete(false),
				      inprogress(false),
				      nukemyself(NULL)
	{
	}

	~simdst() noexcept
	{
	}

	void event(const objrepocopy::batonrequest &msg)

	{
		objrepocopy::batonresponse resp;

		objrepocopysrcinterfaceptr p=src.getptr();

		if (p.null())
			return;
		
		p->event(resp);
	}

	void event(const objrepocopy::masterlist &msg) // [SENDMASTERLIST]

	{
		objrepocopysrcinterfaceptr p=src.getptr();

		if (p.null())
			return;

		std::set<std::string> object_names;

		tobjrepoObj::values_t values;

		objrepocopy::slavelist ack;

		ack.uuids=objuuidlist::create();

		if (nukemyself)
		{
			objrepocopysrcthreadptr src2(p);

			*nukemyself=x::ptr<simdst>();

			src2->event(ack);
			return;
		}

		for (std::map<std::string, x::uuid>::const_iterator
			     b(msg.uuids->objuuids.begin()),
			     e(msg.uuids->objuuids.end()); b != e; ++b)
		{
			object_names.insert(b->first);
		}

		repo->values(object_names, false, values,
			     ack.uuids->objnouuids);

		for (tobjrepoObj::values_t::const_iterator
			     b(values.begin()), e(values.end()); b != e; ++b)
		{
			if (b->second.first ==
			    msg.uuids->objuuids.find(b->first)->second)
				continue;

			ack.uuids->objuuids.insert
				(std::make_pair(b->first,
						b->second.first));
		}

		p->event(ack);
	}

	void event(const objrepocopy::masterlistdone &msg) // [SENDMASTERLIST]

	{
		objrepocopy::slavelistready ack;

		objrepocopysrcinterfaceptr p=src.getptr();

		if (p.null())
			return;
		p->event(ack);
	}

	void event(const objrepocopy::slaveliststart &msg) // [SLAVELISTSTART]

	{
		std::unique_lock<std::mutex> lock(mutex);

		inprogress=true;

		cond.notify_all();

		while (inprogress)
			cond.wait(lock);

		e=x::ptr<objuuidenumeratorObj>::create(repo);

		objrepocopy::masterack dummy;

		event(dummy);
	}

	void event(const objrepocopy::masterack &msg) // [SENDMASTERACK]

	{
		objrepocopysrcinterfaceptr p=src.getptr();

		if (p.null())
			return;


		x::ptr<objuuidlistObj> uuids(e->next());

		if (uuids.null())
		{
			objrepocopy::slavelistdone ack;

			p->event(ack);
			e=x::ptr<objuuidenumeratorObj>();
		}
		else
		{
			objrepocopy::slavelist ack;

			ack.uuids=uuids;
			p->event(ack);
		}
	}

	void event(const objrepocopy::copycomplete &msg) // [SENDCOPYCOMPLETE]

	{
		std::lock_guard<std::mutex> lock(mutex);

		complete=true;

		cond.notify_all();
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

	auto dst=x::ref<simdst>::create(repo2);

	objrepocopysrcthreadptr src=
		objrepocopysrcthreadptr::create("src");

	dst->src=src;

	x::runthreadbaseptr threadrun;

	try {
		x::ptr<x::obj> mcguffin=x::ptr<x::obj>::create();

		auto complete=objrepocopysrcthreadObj::copycomplete
			::create(dst);

		threadrun=x::run(src, repo, batonptr(), complete, mcguffin);

		// [OBJSERIALIZER]

		{
			std::unique_lock<std::mutex> lock(dst->mutex);

			while (!dst->inprogress)
				dst->cond.wait(lock);

			delobj(repo, "obj1");
			uuidmap.erase(uuidmap.find("obj1"));
			uuidmap["newobj1"]=mkobj(repo, "newobj1");

			dst->inprogress=false;
			dst->cond.notify_all();
		}

		{
			x::ptr<x::destroyCallbackFlagObj>
				cb(x::ptr<x::destroyCallbackFlagObj>::create());

			mcguffin->addOnDestroy(cb);
			mcguffin=x::ptr<x::obj>();
			std::cout << "Testing [COPYSRCMCGUFFIN]" << std::endl;
			cb->wait(); // [COPYSRCMCGUFFIN]
		}
		
		if (!complete->success())
			throw EXCEPTION("Did not get success indication");

		delobj(repo, "obj2");
		uuidmap.erase(uuidmap.find("obj2"));
		uuidmap["newobj2"]=mkobj(repo, "newobj2"); // [REPLICATION]

		complete->release();
		threadrun->wait();
	} catch (...)
	{
		if (!threadrun.null())
		{
			src->stop();
			threadrun->wait();
		}
		throw;
	}

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

static void test2()
{
	tobjrepo repo(tobjrepo::create("conftestdir.tst"));
	tobjrepo repo2(tobjrepo::create("conftestdir2.tst"));

	mkobj(repo2, "objx");

	x::ptr<simdst> dst(x::ptr<simdst>::create(repo2));

	objrepocopysrcthreadptr src=objrepocopysrcthreadptr::create("src");

	dst->src=src;

	dst->nukemyself= &dst;

	x::runthreadbaseptr threadrun;

	try {
		x::ptr<x::obj> mcguffin=x::ptr<x::obj>::create();

		auto complete=objrepocopysrcthreadObj::copycomplete
			::create(dst);

		threadrun=x::run(src, repo, batonptr(), complete, mcguffin);

		{
			x::ptr<x::destroyCallbackFlagObj>
				cb(x::ptr<x::destroyCallbackFlagObj>::create());

			mcguffin->addOnDestroy(cb);
			mcguffin=x::ptr<x::obj>();
			std::cout << "Testing [COPYSRCMCGUFFIN]" << std::endl;
			cb->wait();
		}

		if (complete->success())
			throw EXCEPTION("Got a false success indication");

		threadrun->wait();
	} catch (...)
	{
		if (!threadrun.null())
		{
			src->stop();
			threadrun->wait();
		}
		throw;
	}
}

static void test3()
{
	tobjrepo repo(tobjrepo::create("conftestdir.tst"));
	tobjrepo repo2(tobjrepo::create("conftestdir2.tst"));

	mkobj(repo2, "objx");

	auto dst=x::ref<simdst>::create(repo2);

	objrepocopysrc src(objrepocopysrc::create());

	dst->src=src;

	objrepocopysrcthreadObj::copycomplete
		complete(src->start(repo, dst, batonptr(),
				    x::ptr<x::obj>()));
}


class test4_dst : public objrepocopydstinterfaceObj {

public:

	objrepocopysrcinterfaceptr src;

	std::string uuid;

	std::mutex mutex;
	std::condition_variable cond;

	bool something_received;

	test4_dst(const std::string &uuidArg,
		  const objrepocopysrcinterfaceptr &srcArg)

		: src(srcArg), uuid(uuidArg), something_received(false)
	{
	}

	~test4_dst() noexcept
	{
	}

	void event(const objrepocopy::batonrequest &msg)

	{
		objrepocopy::batonresponse resp;

		resp.uuid=uuid;
		src->event(resp);
	}

	void event(const objrepocopy::masterlist &msg)

	{
		std::lock_guard<std::mutex> lock(mutex);
		something_received=true;
		cond.notify_all();
	}

	void event(const objrepocopy::masterlistdone &msg)

	{
		std::lock_guard<std::mutex> lock(mutex);
		something_received=true;
		cond.notify_all();
	}

	void event(const objrepocopy::slaveliststart &msg)

	{
		std::lock_guard<std::mutex> lock(mutex);
		something_received=true;
		cond.notify_all();
	}

	void event(const objrepocopy::masterack &msg)

	{
		std::lock_guard<std::mutex> lock(mutex);
		something_received=true;
		cond.notify_all();
	}

	void event(const objrepocopy::copycomplete &msg)

	{
		std::lock_guard<std::mutex> lock(mutex);
		cond.notify_all();
	}

	void event(const objserializer &msg)

	{
		std::lock_guard<std::mutex> lock(mutex);
		something_received=true;
		cond.notify_all();
	}
};

void test4()
{
	tobjrepo repo(tobjrepo::create("conftestdir.tst"));
	x::ptr<test4_dst> dst;
	objrepocopysrcptr src;
	batonptr batonp;
	x::weakptr<batonptr > wbaton;

	objrepocopysrcObj::copycompleteptr complete;

	src=objrepocopysrcptr::create();
	batonp=batonptr::create("a", x::uuid(), "b", x::uuid());
	dst=x::ptr<test4_dst>::create("", src);

	complete=src->start(repo, dst, batonptr(), x::ptr<x::obj>());

	wbaton=batonp;
	batonp=batonptr();

	{
		std::unique_lock<std::mutex> lock(dst->mutex);

		while (!dst->something_received)
			dst->cond.wait(lock);
	}

	if (!wbaton.getptr().null()) // [BATONSRCOPYRELEASEMISMATCH]
		throw EXCEPTION("[BATONSRCOPYRELEASEMISMATCH] failed");

	src->stop();
	complete=objrepocopysrcObj::copycompleteptr();
	src->wait();

	src=objrepocopysrcptr::create();
	batonp=batonptr::create("a", x::uuid(), "b", x::uuid());
	dst=x::ptr<test4_dst>::create(x::tostring(batonp->batonuuid),
				      src);

	complete=src->start(repo, dst, batonp, x::ptr<x::obj>());

	x::ptr<x::destroyCallbackFlagObj>
		cb(x::ptr<x::destroyCallbackFlagObj>::create());

	batonp->addOnDestroy(cb);
	batonp=batonptr();

	src->wait();
	src=objrepocopysrcptr();
	complete=objrepocopysrcObj::copycompleteptr();

	cb->wait();
	{
		std::lock_guard<std::mutex> lock(dst->mutex);

		// [BATONSRCCOPYRELEASEDONE]
		if (dst->something_received)
			throw EXCEPTION("[BATONSRCCOPYRELEASEDONE] failed");
	}
	    
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
		std::cerr << "test2: expected errors to follow" << std::endl;
		test2();
		std::cerr << "test3" << std::endl;
		test3();
		std::cerr << "test4" << std::endl;
		test4();
		x::dir::base::rmrf("conftestdir.tst");
		x::dir::base::rmrf("conftestdir2.tst");
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

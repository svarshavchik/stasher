/*
** Copyright 2012-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tst.nodes.H"

#include "trandistuuidfwd.H"

static bool break_commit_req=false;

#define REPOPEERCONNECTION_DEBUG_COMMITREQ() do {		\
		if (break_commit_req)				\
		{						\
			throw EXCEPTION("Broken connection");	\
		}						\
	} while (0)


#include "repopeerconnection.C"

static void (*debug_distributor_received_hook)(const trandistuuid &,
					       const std::string &);

#define DEBUG_DISTRIBUTOR_RECEIVED_HOOK(n)				\
	do {								\
		if (debug_distributor_received_hook)			\
			(*debug_distributor_received_hook)		\
				(n, cluster->getthisnodename());	\
	} while (0)


#include "trandistributor.C"
#include "repomg.H"
#include "node.H"

#include <algorithm>
#include <iterator>
#include <x/options.H>
#include <x/destroycallbackflag.H>


class instance : public node {

public:
	instance(const std::string &dir)
		: node(dir)
	{
	}

	void cleanup()
	{
		{
			std::set<std::string> tmpnames;

			objrepoObj *p=dynamic_cast<objrepoObj *>
				((x::obj *)&*repo); // I hate myself

			std::pair<objrepoObj::tmp_iter_t,
				  objrepoObj::tmp_iter_t>
				t(p->tmp_iter());

			std::copy(t.first, t.second,
				  std::insert_iterator<std::set<std::string> >
				  (tmpnames, tmpnames.end()));

			for (std::set<std::string>::iterator b=tmpnames.begin(),
				     e=tmpnames.end(); b != e; ++b)
				p->tmp_remove(*b);
		}

		{
			std::set<std::string> objnames;

			objrepoObj *p=dynamic_cast<objrepoObj *>
				((x::obj *)&*repo);

			for (objrepoObj::obj_iter_t b=p->obj_begin(),
				     e=p->obj_end(); b != e; ++b)
			{
				std::string s=*b;

				if (s.substr(0, 4) == "etc/")
				{
					if (s.substr(4).find('/')
					    == std::string::npos)
						continue; // Keep it
				}
				objnames.insert(s);
			}

			for (std::set<std::string>::iterator b=objnames.begin(),
				     e=objnames.end(); b != e; ++b)
				p->obj_remove(*b, x::ptr<x::obj>());
		}
	}
};


void test1()
{
	instance a(tstnodes::getnodedir(0));

	a.start(true);

	x::uuid truuid[2];

	for (size_t pass=0; pass<2; ++pass)
	{
		std::cout << "PASS " << pass << std::endl;
		trandistributorObj::transtatus stat;
		x::ptr<x::obj> mcguffin(x::ptr<x::obj>::create());

		{
			newtran tr(a.repo->newtransaction());

			(*tr->newobj("foo")->getostream())
				<< "bar" << std::flush;


			stat=a.distributor->newtransaction(tr, mcguffin);
		}

		auto flag=x::destroyCallbackFlag::create();

		mcguffin->ondestroy([flag]{flag->destroyed();});

		mcguffin=nullptr;
		flag->wait(); // [TRANDISTDONE]

		truuid[pass]=stat->uuid; // [TRANDISTNEWUUID]

		if (stat->status != (pass == 0
				     ? STASHER_NAMESPACE::req_processed_stat
				     : STASHER_NAMESPACE::req_rejected_stat))
			throw EXCEPTION("[TRANDISTDONE] failed ([1 of 2])");
	}

	std::list<std::string> objects;

	for (tobjrepoObj::obj_iter_t b(a.repo->obj_begin()),
		     e(a.repo->obj_end()); b != e; ++b)
	{
		std::string s= *b;

		if (s == node::rootcerts)
			continue;

		if (s.substr(0, 4) == "etc/")
			if (s.substr(4).find('/') != std::string::npos)
				continue;

		objects.push_back(s);
	}

	if (objects.size() != 1 || *objects.begin() != "foo")
		throw EXCEPTION("[TRANDISTDONE] failed ([2 of 2])");

	trandistributorObj::transtatus stat;
	x::ptr<x::obj> mcguffin(x::ptr<x::obj>::create());

	{
		newtran tr(a.repo->newtransaction());

		tr->delobj("foo", truuid[0]);
		stat=a.distributor->newtransaction(tr, mcguffin);
	}

	{
		auto flag=x::destroyCallbackFlag::create();

		mcguffin->ondestroy([flag]{flag->destroyed();});

		mcguffin=nullptr;
		flag->wait();
	}

	if (stat->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION("Could not delete commited object");

	objects.clear();

	for (tobjrepoObj::obj_iter_t b(a.repo->obj_begin()),
		     e(a.repo->obj_end()); b != e; ++b)
	{
		std::string s= *b;

		if (s == node::rootcerts)
			continue;

		if (s.substr(0, 4) == "etc/")
			if (s.substr(4).find('/') != std::string::npos)
				continue;

		objects.push_back(s);
	}

	if (objects.size() != 0)
		throw EXCEPTION("Commited object was not deleted");

}

class test2loggerobj : virtual public x::obj {

public:
	test2loggerobj() {}
	~test2loggerobj() noexcept {}

	std::map<std::string, std::list<std::string> > commitlog;
	std::mutex mutex;

	void recordcommit(const std::string &objname,
			  const std::string &nodename)
	{
		std::unique_lock<std::mutex> lock(mutex);

		commitlog[objname].push_back(nodename);
	}

	std::string getcommitorder(const std::string &objname)

	{
		std::ostringstream o;

		const char *sep="";

		std::unique_lock<std::mutex> lock(mutex);

		for (std::list<std::string>::iterator
			     b=commitlog[objname].begin(),
			     e=commitlog[objname].end(); b != e; ++b)
		{
			o << sep << *b;
			sep="/";
		}

		return o.str();
	}
};

class test2enumobj : public objrepoObj::notifierObj {

public:
	tobjrepo repo;
	std::string name;
	x::ptr<test2loggerobj> logger;

	test2enumobj(const tobjrepo &repoArg,
		     const std::string &nameArg,
		     const x::ptr<test2loggerobj> &loggerArg)

		: repo(repoArg), name(nameArg), logger(loggerArg) {}

	~test2enumobj() noexcept {}

	std::set<std::string> names;
	std::mutex mutex;
	std::condition_variable cond;

	void refresh()
	{
		std::unique_lock<std::mutex> lock(mutex);

		names.clear();

		for (tobjrepoObj::obj_iter_t b(repo->obj_begin()),
			     e(repo->obj_end());
		     b != e; ++b)
		{
			std::string n(*b);

			if (n.substr(0, 4) == "etc/")
				continue;
			names.insert(n);
		}
		cond.notify_all();
	}

	void installed(const std::string &objname,
		       const x::ptr<x::obj> &lock)

	{
		refresh();

		logger->recordcommit(objname, name);
	}

	void removed(const std::string &objname,
		     const x::ptr<x::obj> &lock)

	{
		refresh();
	}

	void wait4tranfinish()
	{
		std::unique_lock<std::mutex> lock(mutex);

		while (1)
		{
			std::set<std::string>::iterator b=names.begin(),
				e=names.end();

			while (b != e)
			{
				if (b->find('/') != std::string::npos)
				{
					std::cerr << "Waiting for "
						  << *b << " to go away"
						  << std::endl;
					break;
				}
				++b;
			}

			if (b == e)
				break;

			cond.wait(lock);
		}
	}
};

void test2()
{
	instance a(tstnodes::getnodedir(0));
	instance b(tstnodes::getnodedir(1));
	instance c(tstnodes::getnodedir(2));


        {
                STASHER_NAMESPACE::nodeinfomap clusterinfo;

                STASHER_NAMESPACE::nodeinfo info;

                info.options.insert(std::make_pair(STASHER_NAMESPACE::nodeinfo::host_option,
                                                   "localhost"));

                clusterinfo[a.listener->nodeName()]=info;

		info.nomaster(true);
                clusterinfo[b.listener->nodeName()]=info;
                clusterinfo[c.listener->nodeName()]=info;

                repoclusterinfoObj::saveclusterinfo(a.repo, clusterinfo);
                repoclusterinfoObj::saveclusterinfo(b.repo, clusterinfo);
                repoclusterinfoObj::saveclusterinfo(c.repo, clusterinfo);
        }

	a.start(true);
	b.start(true);
	c.start(true);

	std::cerr << "Started instances" << std::endl;

	// This will establish connections A-B, and A-C
	a.listener->connectpeers();
	a.debugWait4AllConnections();

	a.debugWaitFullQuorumStatus(true);
	b.debugWaitFullQuorumStatus(true);
	c.debugWaitFullQuorumStatus(true);

	std::cerr << "In quorum" << std::endl;
	{
		nodeclusterstatus as=*clusterinfoObj::status(a.repocluster);
		nodeclusterstatus bs=*clusterinfoObj::status(b.repocluster);
		nodeclusterstatus cs=*clusterinfoObj::status(c.repocluster);

		std::string aname=a.listener->nodeName();

		if (as.master != aname || bs.master != aname ||
		    cs.master != aname ||
		    as.uuid != bs.uuid || as.uuid != cs.uuid)
		{
			std::ostringstream o;

			as.toString(o);

			o << "; ";

			bs.toString(o);

			o << "; ";

			cs.toString(o);

			throw EXCEPTION("Cluster did not come up in the expected status: " + o.str());
		}
	}

	// Need to make a B-C connection

	b.listener->connectpeers();
	c.debugWait4AllConnections();

	// ----------------------------------------------------

	tobjrepo repos[3]={a.repo, b.repo, c.repo};
	x::ptr<test2loggerobj> logger(x::ptr<test2loggerobj>::create());
	x::ptr<test2enumobj> repoenums[3];

	repoenums[0]=x::ptr<test2enumobj>::create(repos[0], "a", logger);
	repoenums[1]=x::ptr<test2enumobj>::create(repos[1], "b", logger);
	repoenums[2]=x::ptr<test2enumobj>::create(repos[0], "c", logger);

	repos[0]->installNotifier(repoenums[0]);
	repos[1]->installNotifier(repoenums[1]);
	repos[2]->installNotifier(repoenums[2]);

	repoenums[0]->refresh();
	repoenums[1]->refresh();
	repoenums[2]->refresh();

	trandistributorObj::transtatus stat;
	x::ptr<x::obj> mcguffin(x::ptr<x::obj>::create());

	{
		newtran tr(b.repo->newtransaction());

		(*tr->newobj("foobar1")->getostream())
			<< "baz" << std::flush;

		stat=b.distributor->newtransaction(tr, mcguffin);
	}

	auto flag=x::destroyCallbackFlag::create();

	mcguffin->ondestroy([flag]{flag->destroyed();});

	mcguffin=nullptr;
	flag->wait();

	if (stat->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION("Transaction commit failed");

	for (size_t i=0; i<3; ++i)
	{
		tobjrepoObj::values_t valuesMap;
		std::set<std::string> notfound;
		std::set<std::string> s;

		s.insert("foobar1");

		repos[i]->values(s, false, valuesMap, notfound);

		if (valuesMap.find("foobar1") == valuesMap.end())
			throw EXCEPTION("test2 failed");

		repoenums[i]->wait4tranfinish();
	}

	if (logger->getcommitorder("foobar1") != "c/a/b") // [COMMITORDER]
		throw EXCEPTION("[COMMITORDER] failed");


	// ---

	mcguffin=x::ptr<x::obj>::create();

	{
		newtran tr(b.repo->newtransaction());

		(*tr->newobj("foobar1")->getostream())
			<< "baz" << std::flush;

		stat=b.distributor->newtransaction(tr, mcguffin);
	}

	flag=x::destroyCallbackFlag::create();

	mcguffin->ondestroy([flag]{flag->destroyed();});

	mcguffin=nullptr;
	flag->wait();

	if (stat->status != STASHER_NAMESPACE::req_rejected_stat)
		throw EXCEPTION("Transaction did not fail, as expected");

	for (size_t i=0; i<3; ++i)
	{
		repoenums[i]->wait4tranfinish();
	}
}

void test3()
{
	instance a(tstnodes::getnodedir(0));
	instance b(tstnodes::getnodedir(1));

	std::string bname=b.repocluster->getthisnodename();

        {
                STASHER_NAMESPACE::nodeinfomap clusterinfo;

                STASHER_NAMESPACE::nodeinfo info;

                info.options.insert(std::make_pair(STASHER_NAMESPACE::nodeinfo::host_option,
                                                   "localhost"));

                clusterinfo[a.listener->nodeName()]=info;

		info.nomaster(true);
                clusterinfo[b.listener->nodeName()]=info;

                repoclusterinfoObj::saveclusterinfo(a.repo, clusterinfo);
                repoclusterinfoObj::saveclusterinfo(b.repo, clusterinfo);
        }

	a.start(true);
	b.start(true);

	a.listener->connectpeers();
	a.debugWait4AllConnections();

	a.debugWaitFullQuorumStatus(true);
	b.debugWaitFullQuorumStatus(true);

	x::ptr<test2loggerobj> logger(x::ptr<test2loggerobj>::create());

	auto aenum=x::ref<test2enumobj>::create(a.repo, "a", logger),
		benum=x::ref<test2enumobj>::create(b.repo, "b", logger);


	{
		nodeclusterstatus as=*clusterinfoObj::status(a.repocluster);
		nodeclusterstatus bs=*clusterinfoObj::status(b.repocluster);
		std::string aname=a.listener->nodeName();

		if (as.master != aname || bs.master != aname ||
		    as.uuid != bs.uuid)
		{
			std::ostringstream o;

			as.toString(o);

			o << "; ";

			bs.toString(o);

			throw EXCEPTION("Cluster did not come up in the expected status: " + o.str());
		}
	}

	break_commit_req=true;

	trandistributorObj::transtatus stat;
	x::ptr<x::obj> mcguffin;
	x::destroyCallbackFlagptr flag;

	for (size_t pass=0; pass<2; ++pass)
	{
		std::cerr << "Pass " << pass << ":" << std::endl;

		flag=x::destroyCallbackFlag::create();

		repocontrollermasterptr(a.repocluster->getCurrentController())
			->debugGetPeerConnection(bname)
			->ondestroy([flag]{flag->destroyed();});

		mcguffin=x::ptr<x::obj>::create();

		{
			newtran tr(b.repo->newtransaction());

			(*tr->newobj("test3obj1")->getostream())
				<< "baz" << std::flush;

			stat=b.distributor->newtransaction(tr, mcguffin);
		}

		std::cerr << "Waiting for connection to break" << std::endl;
		flag->wait();

		std::cerr << "Waiting for connection to be reestablished"
			  << std::endl;

		a.listener->connectpeers();
		a.debugWait4AllConnections();

		std::cerr << "Waiting for quorum" << std::endl;

		a.debugWaitFullQuorumStatus(true);
		b.debugWaitFullQuorumStatus(true);

		std::cerr << "Waiting for transaction to complete" << std::endl;

		flag=x::destroyCallbackFlag::create();
		mcguffin->ondestroy([flag]{flag->destroyed();});
		mcguffin=nullptr;
		flag->wait();

		if (stat->status != pass ?
		    STASHER_NAMESPACE::req_rejected_stat
		    : STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION("Transaction did not complete with the expected status");

		std::cerr << "Waiting for transaction to get cleaned up"
			  << std::endl;

		aenum->wait4tranfinish();
		benum->wait4tranfinish();
	}

	break_commit_req=false;
}

static std::mutex test4_mutex;
static std::string test4_node_str;
static bool test4_failed;

static void test4_debug_receivedhook(const trandistuuid &uuid,
				     const std::string &n)
{
	std::unique_lock<std::mutex> lock(test4_mutex);

	if (n != test4_node_str)
		return;

	for (std::map<x::uuid, dist_received_status_t>::iterator
		     b=uuid->uuids.begin(), e=uuid->uuids.end(); b != e; ++b)
	{
		b->second.status=dist_received_status_err;
		test4_failed=true;
	}
}

void test4()
{
	instance a(tstnodes::getnodedir(0));
	instance b(tstnodes::getnodedir(1));
	instance c(tstnodes::getnodedir(2));


        {
                STASHER_NAMESPACE::nodeinfomap clusterinfo;

                STASHER_NAMESPACE::nodeinfo info;

                info.options.insert(std::make_pair(STASHER_NAMESPACE::nodeinfo::host_option,
                                                   "localhost"));

                clusterinfo[a.listener->nodeName()]=info;

		info.nomaster(true);
                clusterinfo[b.listener->nodeName()]=info;
                clusterinfo[c.listener->nodeName()]=info;

                repoclusterinfoObj::saveclusterinfo(a.repo, clusterinfo);
                repoclusterinfoObj::saveclusterinfo(b.repo, clusterinfo);
                repoclusterinfoObj::saveclusterinfo(c.repo, clusterinfo);
        }

	a.cleanup();
	b.cleanup();
	c.cleanup();

	a.start(true);
	b.start(true);
	c.start(true);

	// This will establish connections A-B, and A-C
	a.listener->connectpeers();
	a.debugWait4AllConnections();

	a.debugWaitFullQuorumStatus(true);
	b.debugWaitFullQuorumStatus(true);
	c.debugWaitFullQuorumStatus(true);

	{
		nodeclusterstatus as=*clusterinfoObj::status(a.repocluster);
		nodeclusterstatus bs=*clusterinfoObj::status(b.repocluster);
		nodeclusterstatus cs=*clusterinfoObj::status(c.repocluster);

		std::string aname=a.listener->nodeName();

		if (as.master != aname || bs.master != aname ||
		    cs.master != aname ||
		    as.uuid != bs.uuid || as.uuid != cs.uuid)
		{
			std::ostringstream o;

			as.toString(o);

			o << "; ";

			bs.toString(o);

			o << "; ";

			cs.toString(o);

			throw EXCEPTION("Cluster did not come up in the expected status: " + o.str());
		}
	}

	// Need to make a B-C connection

	b.listener->connectpeers();
	c.debugWait4AllConnections();

	// ----------------------------------------------------

	std::cout << "Installing hook on c" << std::endl;

	debug_distributor_received_hook=test4_debug_receivedhook;

	{
		std::unique_lock<std::mutex> lock(test4_mutex);

		test4_node_str=c.repocluster->getthisnodename();
		test4_failed=false;
	}

	x::ptr<x::obj> mcguffin;
	trandistributorObj::transtatus stat;
	x::destroyCallbackFlagptr flag;

	mcguffin=x::ptr<x::obj>::create();
	{
		newtran tr(b.repo->newtransaction());

		(*tr->newobj("test4obj1")->getostream())
			<< "baz" << std::flush;

		stat=b.distributor->newtransaction(tr, mcguffin);
	}

	flag=x::destroyCallbackFlag::create();
	mcguffin->ondestroy([flag]{flag->destroyed();});
	mcguffin=nullptr;
	std::cout << "Waiting for a fail" << std::endl;
	flag->wait();

	{
		std::unique_lock<std::mutex> lock(test4_mutex);

		if (stat->status != STASHER_NAMESPACE::req_failed_stat ||
		    !test4_failed) // [COMMITFAILONRCPT]
			throw EXCEPTION("[COMMITFAILONRCPT] failed");
		std::cout << "Installing hook on a" << std::endl;
		test4_node_str=a.repocluster->getthisnodename();
		test4_failed=false;
	}

	mcguffin=x::ptr<x::obj>::create();
	{
		newtran tr(b.repo->newtransaction());

		(*tr->newobj("test4obj2")->getostream())
			<< "baz" << std::flush;

		stat=b.distributor->newtransaction(tr, mcguffin);
	}

	flag=x::destroyCallbackFlag::create();
	mcguffin->ondestroy([flag]{flag->destroyed();});
	mcguffin=nullptr;
	std::cout << "Waiting for a fail" << std::endl;
	flag->wait();

	{
		std::unique_lock<std::mutex> lock(test4_mutex);

		if (stat->status != STASHER_NAMESPACE::req_failed_stat ||
		    !test4_failed) // [COMMITFAILONRCPT]
			throw EXCEPTION("[COMMITFAILONRCPT] failed");
	}

	debug_distributor_received_hook=NULL;
}

void test5()
{
	instance a(tstnodes::getnodedir(0));

	uint64_t s=a.listener->df()->freespacemb();
	uint64_t i=a.listener->df()->freeinodes();

	a.listener->df()
		->reservespace_alloc(a.listener->df()->calculate_alloc(1024L
								       * 1024L
								       * 10),
				       10)->commit();

	uint64_t s2=a.listener->df()->freespacemb();
	uint64_t i2=a.listener->df()->freeinodes();

	if (s == s2 && i == i2)
		throw EXCEPTION("test5 failed");

	while (s2 == a.listener->df()->freespacemb() &&
	       i2 == a.listener->df()->freeinodes())
		a.listener->df()->reservespace_alloc(1, 1);
}

void test6()
{
	instance a(tstnodes::getnodedir(0));

	uint64_t i=a.listener->df()->freeinodes()/4*3;

	spacemonitorObj::reservationptr
		resa=a.listener->df()->reservespace_alloc(0, i),
		resb=a.listener->df()->reservespace_alloc(0, i);

	if (!a.listener->df()->outofspace() ||
	    !a.listener->df()->outofspace())
		throw EXCEPTION("test6 failed (1)");

	resb=spacemonitorObj::reservationptr();

	if (a.listener->df()->outofspace())
		throw EXCEPTION("test6 failed (2)");
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		tstnodes nodes(3);
		ALARM(120);

		std::cout << "test1" << std::endl;
		test1();

		exit(0);

		std::cout << "test2" << std::endl;
		test2();

		std::cout << "test3" << std::endl;
		test3();

		std::cout << "test4" << std::endl;
		test4();

		if (LIBCXX_NAMESPACE::property::value<bool>("extratests",
							    false).getValue())
		{
			std::cout << "test5" << std::endl;
			test5();

			std::cout << "test6" << std::endl;
			test6();
		}
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

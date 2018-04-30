/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"

#define DEBUG_DISABLE_MASTER_COMMITS
#include "repocontrollermaster.C"

#include "repoclusterinfoimpl.H"
#include "clusterlistenerimpl.H"
#include "stoppablethreadtracker.H"
#include "clustertlsconnectshutdown.H"
#include "trandistributor.H"
#include "repomg.H"
#include "newtran.H"
#include "trancommit.H"
#include <x/options.H>

class mytlsshutdown : public clustertlsconnectshutdownObj {

public:
	mytlsshutdown() {}
	~mytlsshutdown() {}

	static std::mutex mutex;
	static std::condition_variable cond;
	static size_t counter;

	void run(x::ptr<x::obj> &mcguffin,
		 const x::fd &socket,
		 const x::gnutls::session &session) override
	{
		clustertlsconnectshutdownObj::run(mcguffin, socket, session);

		std::unique_lock<std::mutex> lock(mutex);

		++counter;
		cond.notify_all();
	}
};

std::mutex mytlsshutdown::mutex;
std::condition_variable mytlsshutdown::cond;
size_t mytlsshutdown::counter=0;

class quorumcbObj: public STASHER_NAMESPACE::repoclusterquorumcallbackbaseObj {

public:
	std::mutex mutex;
	std::condition_variable cond;
	bool flag;

	quorumcbObj() : flag(false)
	{
	}

	~quorumcbObj()
	{
	}

	void quorum(const STASHER_NAMESPACE::quorumstate &inquorum)
		override
	{
		std::unique_lock<std::mutex> lock(mutex);

		flag=inquorum.full;
		cond.notify_all();
	}

	void wait4(bool forwhat)
	{
		std::unique_lock<std::mutex> lock(mutex);

		while (flag != forwhat)
			cond.wait(lock);
	}
};

static const char clustername[]="test";
static const char clusterdir[]="conftestcluster1.dir";
static const char nodea[]="nodea";
static const char nodeb[]="nodeb";
static const char nodeadir[]="conftestnodea.dir";
static const char nodebdir[]="conftestnodeb.dir";

class instancebase {

public:
	tobjrepo repo;

	instancebase(const std::string &dir)
		: repo(tobjrepo::create(dir))
	{
		STASHER_NAMESPACE::nodeinfomap clusterinfo;

		repoclusterinfoObj::saveclusterinfo(repo, clusterinfo);
	}
};

class instance : public instancebase {

public:
	STASHER_NAMESPACE::stoppableThreadTrackerImpl trackerimpl;
	STASHER_NAMESPACE::stoppableThreadTracker tracker;

	clusterlistenerimpl listener;
	repoclusterinfoimpl repocluster;
	x::ptr<quorumcbObj> quorumstatus;

	instance(const std::string &dir)
		: instancebase(dir),
		  trackerimpl(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create()),
		  tracker(trackerimpl->getTracker()),
		  listener(clusterlistenerimpl::create(dir)),
		  repocluster(repoclusterinfoimpl::create
			      (listener->nodeName(),
			       listener->clusterName(), repo,
			       x::ptr<trandistributorObj>(), tracker)),
		  quorumstatus(x::ptr<quorumcbObj>::create())
	{
		repocluster->initialize();
		tracker->start_threadmsgdispatcher(listener,
				      tracker,
				      x::ptr<trandistributorObj>(),
				      repo,
				      x::ptr<mytlsshutdown>::create(),
				      repocluster);

		repocluster->installQuorumNotification(quorumstatus);

		(void)repocluster->debug_inquorum();
		quorumstatus->wait4(true);
	}

	~instance()
	{
	}
};

static void test1()
{
	instance a(nodeadir);
	instance b(nodebdir);

	std::cout << "started instances" << std::endl;

	{
		newtran tran(b.repo->newtransaction());

		(*tran->newobj("obj1")->getostream()) << "obj1" << std::endl;

		b.repo->begin_commit(tran->finalize(),
				     x::eventfd::create())->commit();
	}

	{
		newtran tran(a.repo->newtransaction());

		(*tran->newobj("obj2")->getostream()) << "obj2" << std::endl;

		a.repo->begin_commit(tran->finalize(),
				     x::eventfd::create())->commit();
	}

	{
		STASHER_NAMESPACE::nodeinfomap clusterinfo;

		STASHER_NAMESPACE::nodeinfo &ainfo=clusterinfo[a.listener->nodeName()];

		ainfo.options
			.insert(std::make_pair(STASHER_NAMESPACE::nodeinfo::host_option,
					       "localhost"));
		ainfo.nomaster(true);

		clusterinfo[b.listener->nodeName()].options
			.insert(std::make_pair(STASHER_NAMESPACE::nodeinfo::host_option,
					       "localhost"));

		repoclusterinfoObj::saveclusterinfo(b.repo, clusterinfo);
		repoclusterinfoObj::saveclusterinfo(a.repo, clusterinfo);
	}

	a.quorumstatus->wait4(false);
	b.quorumstatus->wait4(false);

	std::cout << "updated host connection parameters" << std::endl;

	b.listener->connectpeers();

	// [QUORUMDIST]
	b.quorumstatus->wait4(true);
	a.quorumstatus->wait4(true);

	std::cout << "a: " << ({
			std::ostringstream o;

			clusterinfoObj::status(a.repocluster)->toString(o);

			o.str(); }) << std::endl;

	std::cout << "b: " << ({
			std::ostringstream o;

			clusterinfoObj::status(b.repocluster)->toString(o);

			o.str(); }) << std::endl;

	tobjrepoObj::values_t valuesMap;

	std::set<std::string> notfound;
	{
		std::set<std::string> s;

		s.insert("obj1");
		s.insert("obj2");

		a.repo->values(s, true, valuesMap, notfound);
	}

	if (notfound.size() != 1 || notfound.find("obj2") == notfound.end())
		throw EXCEPTION("test1: obj2 was not removed by syncing");

	std::string s;

	if (valuesMap.size() == 1 && valuesMap.find("obj1") != valuesMap.end()
	    && (std::getline(*valuesMap.find("obj1")->second.second
			     ->getistream(), s), s) == "obj1")
		return;
	throw EXCEPTION("test1: obj1 was not synced over");
}

static void test2()
{
	mytlsshutdown::counter=0;

	instance a(nodeadir);
	instance b(nodebdir);

	std::cout << "started instances" << std::endl;

	{
		STASHER_NAMESPACE::nodeinfomap clusterinfo;

		STASHER_NAMESPACE::nodeinfo &ainfo=clusterinfo[a.listener->nodeName()];

		ainfo.options
			.insert(std::make_pair(STASHER_NAMESPACE::nodeinfo::host_option,
					       "localhost"));
		ainfo.nomaster(true);
		ainfo.useencryption(true);

		STASHER_NAMESPACE::nodeinfo &binfo=clusterinfo[b.listener->nodeName()];

		binfo.options
			.insert(std::make_pair(STASHER_NAMESPACE::nodeinfo::host_option,
					       "localhost"));
		binfo.useencryption(true);

		repoclusterinfoObj::saveclusterinfo(b.repo, clusterinfo);
		repoclusterinfoObj::saveclusterinfo(a.repo, clusterinfo);
	}

	a.quorumstatus->wait4(false);
	b.quorumstatus->wait4(false);

	std::cout << "updated host connection parameters" << std::endl;

	b.listener->connectpeers();
	b.quorumstatus->wait4(true);
	a.quorumstatus->wait4(true);

	std::cout << "a: " << ({
			std::ostringstream o;

			clusterinfoObj::status(a.repocluster)->toString(o);

			o.str(); }) << std::endl;

	std::cout << "b: " << ({
			std::ostringstream o;

			clusterinfoObj::status(b.repocluster)->toString(o);

			o.str(); }) << std::endl;

	{
		STASHER_NAMESPACE::nodeinfomap clusterinfo;

		repoclusterinfoObj::saveclusterinfo(a.repo, clusterinfo);
	}

	std::unique_lock<std::mutex> lock(mytlsshutdown::mutex);

	while (mytlsshutdown::counter != 2)
		mytlsshutdown::cond.wait(lock);
}

class conn {

public:
	x::fd sock;
	x::gnutls::session sess;

	conn(const x::fd &sockArg,
	     const x::gnutls::session &sessArg) : sock(sockArg), sess(sessArg)
	{
	}


};

static std::pair<conn, conn> makeconn()
{
	x::gnutls::credentials::certificate
		acert(clusterlistenerimpl::create(nodeadir)->credentials()),
		bcert(clusterlistenerimpl::create(nodebdir)->credentials());

	std::pair<x::fd, x::fd> socks(x::fd::base::socketpair());

	x::gnutls::session cl(x::gnutls::session::create(GNUTLS_CLIENT,
							 socks.first)),
		se(x::gnutls::session::create(GNUTLS_SERVER, socks.second));

	cl->set_default_priority();
	cl->credentials_set(acert);

	se->set_default_priority();
	se->server_set_certificate_request();
	se->credentials_set(bcert);

	return std::make_pair(conn(socks.first, cl), conn(socks.second, se));

}

class test345_shutdown : public clustertlsconnectshutdownObj {

public:
	conn c;
	unsigned timeout;

	test345_shutdown(const conn &cArg) : c(cArg) {}

	void run()
	{
		int dummy;

		c.sess->handshake(dummy);
		c.sock->nonblock(true);

		auto mcguffin=x::ptr<x::obj>::create();

		clustertlsconnectshutdownObj::run(mcguffin, c.sock, c.sess);
	}

	unsigned getTimeout() override
	{
		return timeout;
	}
};

void test3()
{
	STASHER_NAMESPACE::stoppableThreadTrackerImpl
		tracker(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create());
	std::pair<conn, conn> dummyconn(makeconn());

	x::ref<test345_shutdown> thr(x::ref<test345_shutdown>
				     ::create(dummyconn.second));
	thr->timeout=1;

	auto runthread=tracker->start(thr);

	int dummy;

	dummyconn.first.sess->handshake(dummy);
	runthread->wait();
}

void test4()
{
	STASHER_NAMESPACE::stoppableThreadTrackerImpl
		tracker(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create());
	std::pair<conn, conn> dummyconn(makeconn());

	x::ref<test345_shutdown> thr(x::ref<test345_shutdown>
				     ::create(dummyconn.second));
	thr->timeout=30;

	auto runthread=tracker->start(thr);

	int dummy;

	dummyconn.first.sess->handshake(dummy);

	dummyconn.first.sock->close();
	runthread->wait();
}

void test5()
{
	STASHER_NAMESPACE::stoppableThreadTrackerImpl
		tracker(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create());
	std::pair<conn, conn> dummyconn(makeconn());

	x::ref<test345_shutdown> thr(x::ref<test345_shutdown>
				     ::create(dummyconn.second));
	thr->timeout=30;

	auto runthread=tracker->start(thr);

	int dummy;

	dummyconn.first.sess->handshake(dummy);

	thr->stop();
	runthread->wait();
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		x::dir::base::rmrf(clusterdir);
		x::dir::base::rmrf(nodeadir);
		x::dir::base::rmrf(nodebdir);

		{
			time_t now(time(NULL));

			repomg::clustkey_generate(clusterdir, clustername,
						  now,
						  now + 365 * 24 * 60 * 60,
						  "rsa",
						  "medium",
						  "sha1");

			repomg::nodekey_generate(nodeadir, clusterdir, "",
						 nodea,
						 now, now + 7 * 24 * 60 * 60,
						 "medium",
						 "sha1");

			repomg::nodekey_generate(nodebdir, clusterdir, "",
						 nodeb,
						 now, now + 7 * 24 * 60 * 60,
						 "medium",
						 "sha1");
		}
		ALARM(120);

		std::cout << "test1" << std::endl;
		test1();
		std::cout << "test2" << std::endl;
		test2();
		std::cout << "test3" << std::endl;
		test3();
		std::cout << "test4" << std::endl;
		test4();
		std::cout << "test5" << std::endl;
		test5();
		x::dir::base::rmrf(clusterdir);
		x::dir::base::rmrf(nodeadir);
		x::dir::base::rmrf(nodebdir);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

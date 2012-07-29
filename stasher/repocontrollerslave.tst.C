/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"

#include <mutex>
#include <condition_variable>

static std::mutex test3_mutex;
static std::condition_variable test3_cond;

static bool test3_flag;

#define DEBUG_SYNCCOPY_COMPLETED()			\
	do {						\
		std::lock_guard<std::mutex> lock(test3_mutex);	\
		test3_flag=true;			\
		test3_cond.notify_all();			\
	} while (0)

#include "repocontrollerslave.C"
#include "slavesyncinfo.H"
#include "repopeerconnection.H"
#include "trandistributor.H"
#include "clusterlistener.H"
#include "peerstatus.H"
#include "tobjrepo.H"
#include "newtran.H"
#include "trancommit.H"
#include "baton.H"
#include "objserializer.H"
#include "objrepocopysrc.H"
#include "objsource.H"
#include "spacemonitor.H"
#include <x/options.H>
#include <x/dir.H>
#include <x/serialize.H>
#include <x/deserialize.H>
#include <x/destroycallbackflag.H>

#include <algorithm>

class dummyhalt : public x::stoppableObj {

public:
	dummyhalt() {}
	~dummyhalt() noexcept {}

	void stop() {
	}
};

class handofftest : public repocontrollerbaseObj {

public:
	handofftest()
	: repocontrollerbaseObj("dummy",
				x::uuid(),
				tobjrepo::create("repo.tst"),
				repoclusterquorum(new repoclusterquorumObj))
	{
	}

	~handofftest() noexcept
	{
	}

	void get_quorum(const STASHER_NAMESPACE::quorumstateref &status_arg,
			const boolref &processed_arg,
			const x::ptr<x::obj> &mcguffin_arg)
	{
		static_cast<STASHER_NAMESPACE::quorumstate &>(*status_arg)=
			STASHER_NAMESPACE::quorumstate();
		processed_arg->flag=true;
	}

	std::mutex mutex;
	std::condition_variable cond;
	x::ptr<x::obj> mcguffin;

	start_controller_ret_t
	start_controller(const x::ref<x::obj> &mcguffinArg)

	{
		std::lock_guard<std::mutex> lock(mutex);

		mcguffin=mcguffinArg;
		cond.notify_all();

		return start_controller_ret_t::create();
	}

	void handoff(const x::ptr<repocontrollerbaseObj> &next)

	{
	}

	void peernewmaster(const repopeerconnectionptr &peerRef,
			   const nodeclusterstatus &peerStatus)

	{
	}

	x::ptr<x::obj>
	handoff_request(const std::string &peername)
	{
		return x::ptr<x::obj>();
	}

	void halt(const STASHER_NAMESPACE::haltrequestresults &req,
		  const x::ref<x::obj> &mcguffin)
	{
	}

};

class test1peer : public repopeerconnectionbaseObj {

public:
	test1peer()
	{
	}

	~test1peer() noexcept
	{
	}

	void connect_slave(const
			   x::ptr<slavesyncinfoObj>
			   &synchandle)

	{
	}

	void connect_master(const
			   x::ref<mastersyncinfoObj>
			   &synchandle)

	{
	}

	void connect_peer(const peerlinkptr &masterlinkArg)

	{
	}

	void disconnect_peer()

	{
	}
};

static const char clusterdir[]="conftestcluster.dir";
static const char cluster2dir[]="conftestcluster2.dir";

static void test1()
{
	std::string peername("nodea");
	x::ptr<test1peer> peer(x::ptr<test1peer>::create());
	x::uuid peeruuid;
	tobjrepo repo(tobjrepo::create(clusterdir));
	STASHER_NAMESPACE::stoppableThreadTrackerImpl
		trackerImpl(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create());
	STASHER_NAMESPACE::stoppableThreadTracker tracker(trackerImpl->getTracker());

	auto slave=x::ptr<repocontrollerslaveObj>
		::create(peername, peer, peeruuid,
			 repo, repoclusterquorum::create(),
			 x::ptr<trandistributorObj>(),
			 tracker, x::ref<dummyhalt>::create());

	x::weakptr<x::ptr<x::obj> > weakmcguffin;

	auto slave_ret=({

			x::ref<x::obj> mcguffin=x::ref<x::obj>::create();

			weakmcguffin=mcguffin;

			slave->start_controller(mcguffin);
		});

	x::ref<handofftest> handoff(x::ref<handofftest>::create());

	slave->handoff(handoff); // [SLAVEHANDOFF]
	slave_ret->wait();
	slave=x::ptr<repocontrollerslaveObj>();

	std::lock_guard<std::mutex> lock(handoff->mutex);

	if (weakmcguffin.getptr().null() ||
	    handoff->mcguffin.null())
		throw EXCEPTION("[SLAVEHANDOFF] failed (1 of 2)");

	handoff->mcguffin=x::ptr<x::obj>();

	if (!weakmcguffin.getptr().null())
		throw EXCEPTION("[SLAVEHANDOFF] failed (2 of 2)");
}

class test2peer : public repopeerconnectionbaseObj {

public:
	objrepocopysrcptr srccopy;

	test2peer() : srccopy(objrepocopysrcptr::create())
	{
	}

	~test2peer() noexcept {}

	std::mutex mutex;
	std::condition_variable cond;
	slavesyncinfo synchandle;

	void connect_master(const
			   x::ref<mastersyncinfoObj>
			   &synchandle)

	{
	}

	void connect_slave(const slavesyncinfo
			   &synchandleArg)

	{
		std::lock_guard<std::mutex> lock(mutex);

		x::ptr<x::obj> mcguffin=synchandleArg->dstmcguffin;

		synchandleArg->dstmcguffin=x::ptr<x::obj>();

		synchandle=synchandleArg;

		objrepocopydstptr dst(synchandle->dstinterface.getptr());

		if (!dst.null())
		{
			dst->start(synchandle->repo,
				   srccopy,
				   synchandle->dstflag,
				   batonptr(),
				   mcguffin);
		}
		cond.notify_all();
	}

	void connect_peer(const peerlinkptr &masterlinkArg)

	{
	}

	void disconnect_peer()

	{
	}
};

class test2_init {

public:
	std::string peername;
	x::ptr<test2peer> peer;
	x::uuid peeruuid;
	tobjrepo repo;
	STASHER_NAMESPACE::stoppableThreadTrackerImpl trackerImpl;
	STASHER_NAMESPACE::stoppableThreadTracker tracker;

	repoclusterquorum quorum_callback_list;

	x::ref<repocontrollerslaveObj> slave;

	test2_init() : peername("nodea"),
		       peer(x::ptr<test2peer>::create()),
		       repo(tobjrepo::create(clusterdir)),
		       trackerImpl(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create()),
		       tracker(trackerImpl->getTracker()),
		       quorum_callback_list(repoclusterquorum::create()),
		       slave(x::ref<repocontrollerslaveObj>::create
			     (peername, peer,
			      peeruuid,
			      repo,
			      quorum_callback_list,
			      x::ptr<trandistributorObj>(),
			      tracker, x::ref<dummyhalt>::create()))
	{
		slave->start_controller(x::ptr<x::obj>::create());

		std::cout << "started slave, waiting for a handle"
			  << std::endl;

		std::unique_lock<std::mutex> lock(peer->mutex);

		while (peer->synchandle.null())
			peer->cond.wait(lock);

		std::cout << "handle received" << std::endl;
	}
};

class test2dstinterface : public objrepocopydstinterfaceObj {

public:

	slavesyncinfo synchandle;
	test2dstinterface(const slavesyncinfo &synchandleArg)
 : synchandle(synchandleArg)
	{
	}

	~test2dstinterface() noexcept
	{
	}

	void event(const objrepocopy::batonrequest &msg)

	{
		objrepocopydstinterfaceptr
			ptr(synchandle->dstinterface.getptr());

		if (!ptr.null())
			ptr->event(msg);
	}

	void event(const objrepocopy::masterlist &msg)

	{
		objrepocopydstinterfaceptr
			ptr(synchandle->dstinterface.getptr());

		if (!ptr.null())
			ptr->event(msg);
	}

	void event(const objrepocopy::masterlistdone &msg)

	{
		objrepocopydstinterfaceptr
			ptr(synchandle->dstinterface.getptr());

		if (!ptr.null())
			ptr->event(msg);
	}

	void event(const objrepocopy::slaveliststart &msg)

	{
		objrepocopydstinterfaceptr
			ptr(synchandle->dstinterface.getptr());

		if (!ptr.null())
			ptr->event(msg);
	}

	void event(const objrepocopy::masterack &msg)

	{
		objrepocopydstinterfaceptr
			ptr(synchandle->dstinterface.getptr());

		if (!ptr.null())
			ptr->event(msg);
	}

	void event(const objrepocopy::copycomplete &msg)

	{
		objrepocopydstinterfaceptr
			ptr(synchandle->dstinterface.getptr());

		if (!ptr.null())
			ptr->event(msg);
	}

	void event(const objserializer &msg)

	{
		objrepocopydstinterfaceptr
			ptr(synchandle->dstinterface.getptr());

		if (ptr.null())
			return;

		std::vector<char> buf;

		{
			typedef std::back_insert_iterator<std::vector<char> >
				buf_iter_t;

			buf_iter_t buf_iter(buf);

			x::serialize::iterator<buf_iter_t>
				ser_iter(buf_iter);

			ser_iter(msg);
		}

		objserializer objdeser(synchandle->repo, "dummy");

		{
			typedef std::vector<char>::iterator buf_iter_t;

			buf_iter_t b(buf.begin()), e(buf.end());

			x::deserialize::iterator<buf_iter_t>
				deser_iter(b, e);

			deser_iter(objdeser);
		}
	}
};

class quorumcallback : public STASHER_NAMESPACE::repoclusterquorumcallbackbaseObj {

public:
	quorumcallback() : flag(false) {}

	~quorumcallback() noexcept {}

	std::mutex mutex;
	std::condition_variable cond;
	bool flag;

	void quorum(const STASHER_NAMESPACE::quorumstate
		    &quorum)
	{
		if (quorum.full)
		{
			std::lock_guard<std::mutex> lock(mutex);
			flag=true;
			cond.notify_all();
		}
	}
};

static void test2()
{
	test2_init test;

	tobjrepo repo2(tobjrepo::create(cluster2dir));

	{
		newtran tr(repo2->newtransaction());

		x::fd fd(tr->newobj("obj1"));

		(*fd->getostream()) << "obj1" << std::endl << std::flush;

		fd->close();

		fd=tr->newobj("obj2");
		(*fd->getostream()) << "obj2" << std::endl << std::flush;

		fd->close();

		repo2->begin_commit(tr->finalize(), x::eventfd::create())
			->commit();
	}

	x::ptr<test2dstinterface>
		dstinterface(new test2dstinterface(test.peer->synchandle));

	x::ptr<x::obj> mcguffin=x::ptr<x::obj>::create();

	objrepocopysrcObj::copycomplete complete(test.peer->srccopy
						 ->start(repo2,
							 dstinterface,
							 batonptr(),
							 mcguffin));
	std::cout << "waiting for source thread" << std::endl;

	{
		x::ptr<x::destroyCallbackFlagObj>
			cb(x::ptr<x::destroyCallbackFlagObj>::create());

		mcguffin->addOnDestroy(cb);
		mcguffin=x::ptr<x::obj>();
		cb->wait();
	}

	if (!complete->success())
		throw EXCEPTION("objrepocopysrc failed");

	{
		newtran tr(repo2->newtransaction());

		tr->delobj("obj2", x::uuid());
		repo2->begin_commit(tr->finalize(), x::eventfd::create())
			->commit();
	}

	complete->release();

	test.slave->master_quorum_announce(STASHER_NAMESPACE::quorumstate(true,
									  true)
					   );

	std::cout << "waiting for destination thread" << std::endl;

	while (1)
	{
		x::ptr<quorumcallback> cb(x::ptr<quorumcallback>::create());

		test.quorum_callback_list->install(cb);

		if (test.slave->debug_inquorum().full)
			break; // [SLAVEQUORUM]

		std::unique_lock<std::mutex> lock(cb->mutex);

		while (!cb->flag)
			cb->cond.wait(lock);
	}

	tobjrepoObj::values_t values;

	{
		std::set<std::string> objects, notfound;

		objects.insert("obj1");
		objects.insert("obj2");

		test.repo->values(objects, true, values, notfound);
	}

	std::string s;

	if (values.size() != 1 ||
	    values.begin()->first != "obj1" ||

	    (std::getline(*values.begin()->second.second->getistream(), s), s)
	    != "obj1")
		throw EXCEPTION("Repository sync failure");

}

class test3peer : public repopeerconnectionObj {

public:
	using repopeerconnectionObj::peerstatusupdate;
	using repopeerconnectionObj::slavesyncrequest;
	using repopeerconnectionObj::thisstatus;

	test3peer(const std::string &peername)
		: repopeerconnectionObj(peername, spacemonitor
					(new spacemonitorObj
					 (x::df::create("."))))
	{
	}

	~test3peer() noexcept {}
};

class test3deser : public STASHER_NAMESPACE::fdobjrwthreadObj {

public:
	objrepocopysrcinterfaceptr src;

	std::mutex mutex;
	std::condition_variable cond;
	bool copy_requested;

	std::string mastername;
	x::uuid masteruuid;
	test3deser(const objrepocopysrcinterfaceptr &srcArg,
		   const std::string &masternameArg,
		   const x::uuid &masteruuidArg)
		: STASHER_NAMESPACE::fdobjrwthreadObj("master"),
		  src(srcArg), copy_requested(false),
		  mastername(masternameArg),
		  masteruuid(masteruuidArg)
	{
	}

	~test3deser() noexcept {}

	template<typename obj>
	class deser : public obj {

	public:
		deser(test3deser &dummy) {}
		~deser() noexcept {}
	};

	template<typename iter_type>
	static void classlist(iter_type &iter)
	{
		iter.template serialize<objrepocopy::slavelist,
					deser<objrepocopy::slavelist> >();

		iter.template serialize<objrepocopy::slavelistready,
					deser<objrepocopy::slavelistready> >();

		iter.template serialize<objrepocopy::slavelistdone,
					deser<objrepocopy::slavelistdone> >();

		iter.template serialize<nodeclusterstatus,
					deser<nodeclusterstatus> >();

		iter.template serialize<test3peer::slavesyncrequest,
					deser<test3peer::slavesyncrequest> >();
	}

	void deserialized(const test3peer::slavesyncrequest &request)

	{
		std::cout << "master <- slavesyncrequest" << std::endl;

		std::lock_guard<std::mutex> lock(mutex);

		copy_requested=true;
		cond.notify_all();
	}

	void deserialized(const nodeclusterstatus &msg)

	{
		std::cout << "master <- nodeclusterstatus" << std::endl;
	}

	void deserialized(const objrepocopy::slavelist &msg)

	{
		std::cout << "master <- slavelist" << std::endl;
		src->event(msg);
	}

	void deserialized(const objrepocopy::slavelistready &msg)

	{
		std::cout << "master <- slavelistready" << std::endl;
		src->event(msg);
	}

	void deserialized(const objrepocopy::slavelistdone &msg)

	{
		std::cout << "master <- slavelistdone" << std::endl;
		src->event(msg);
	}

	void run(const x::fdbase &transport,
		 const x::fd::base::inputiter &inputiter,
		 const STASHER_NAMESPACE::stoppableThreadTracker &tracker,
		 const x::ptr<x::obj> &mcguffin)

	{
		mainloop(transport, inputiter, tracker, mcguffin);
	}

	template<typename msg_type> void submit(const msg_type &msg)

	{
		sendevent<msg_type>(this, msg);
	}

	template<typename obj_type> void dispatch(const obj_type &msg)

	{
		writer->write( x::ref<STASHER_NAMESPACE::writtenObj<obj_type> > ::create(msg));
	}

	MAINLOOP_DECL;
};

MAINLOOP_IMPL(test3deser)

class test3serObj : public objrepocopydstinterfaceObj {

public:

	x::ptr<test3deser> deser;

	test3serObj(const x::ptr<test3deser> &deserArg)
 : deser(deserArg)
	{
	}

	~test3serObj() noexcept
	{
	}

	void event(const objrepocopy::batonrequest &msg)

	{
		objrepocopy::batonresponse resp;

		std::cout << "master -> batonrequest" << std::endl;

		deser->src->event(resp);
	}

	void event(const objrepocopy::masterlist &msg)

	{
		std::cout << "master -> masterlist" << std::endl;
		deser->submit(msg);
	}

	void event(const objrepocopy::masterlistdone &msg)

	{
		std::cout << "master -> masterlistdone" << std::endl;
		deser->submit(msg);
	}

	void event(const objrepocopy::slaveliststart &msg)

	{
		std::cout << "master -> slaveliststart" << std::endl;
		deser->submit(msg);
	}

	void event(const objrepocopy::masterack &msg)

	{
		std::cout << "master -> masterack" << std::endl;
		deser->submit(msg);
	}

	void event(const objrepocopy::copycomplete &msg)

	{
		std::cout << "master -> copycomplete" << std::endl;
		deser->submit(msg);
	}

	void event(const objserializer &msg)

	{
		std::cout << "master -> object("
			  << msg.getName() << ")" << std::endl;
		deser->submit(msg);
	}
};

class test3quorumcb :
	public STASHER_NAMESPACE::repoclusterquorumcallbackbaseObj {

public:
	test3quorumcb() : flag(false) {}
	~test3quorumcb() noexcept {}

	std::mutex mutex;
	std::condition_variable cond;
	bool flag;

	void quorum(const STASHER_NAMESPACE::quorumstate &quorum)

	{
		if (quorum.full)
		{
			std::lock_guard<std::mutex> lock(mutex);
			flag=true;
			cond.notify_all();
		}
	}

	void wait()
	{
		std::unique_lock<std::mutex> lock(mutex);

		while (!flag)
			cond.wait(lock);
		flag=false;
	}
};

template<typename ref_type>
void wait_mcguffin(ref_type &ptr)
{
	x::ptr<x::obj> obj(ptr);

	ptr=ref_type();

	x::destroyCallbackFlag flag(x::destroyCallbackFlag::create());

	obj->addOnDestroy(flag);

	obj=x::ptr<x::obj>();

	flag->wait();
}

static void test3()
{
	std::string peername("nodea");
	x::uuid peeruuid;
	STASHER_NAMESPACE::stoppableThreadTrackerImpl
		tracker(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create());
	tobjrepo dstrepo(tobjrepo::create(clusterdir));
	tobjrepo srcrepo(tobjrepo::create(cluster2dir));

	{
		newtran tr(dstrepo->newtransaction());

		x::fd fd(tr->newobj("obj1"));

		(*fd->getostream()) << "obj1" << std::endl << std::flush;

		fd->close();
		dstrepo->begin_commit(tr->finalize(), x::eventfd::create())
			->commit();
	}

	{
		newtran tr(srcrepo->newtransaction());

		x::fd fd(tr->newobj("obj2"));

		(*fd->getostream()) << "obj2" << std::endl << std::flush;

		fd->close();

		srcrepo->begin_commit(tr->finalize(), x::eventfd::create())
			->commit();

	}

	repoclusterquorum quorum_callback_list(new repoclusterquorumObj);

	x::ptr<test3quorumcb> quorumcb(x::ptr<test3quorumcb>::create());

	quorum_callback_list->install(quorumcb);

	x::ref<test3peer> peer(x::ref<test3peer>::create(peername));
	x::ptr<x::obj> peer_mcguffin(x::ptr<x::obj>::create());

	x::fdptr sock;

	{
		std::pair<x::fd, x::fd> socks(x::fd::base::socketpair());

		socks.first->nonblock(true);
		socks.second->nonblock(true);

		sock=socks.first;

		tracker->start(peer, socks.second,
			       x::fd::base::inputiter(socks.second),
			       tracker->getTracker(),
			       peer_mcguffin,
			       false,
			       x::ptr<trandistributorObj>(),
			       clusterlistenerptr(),
			       nodeclusterstatus(),
			       clusterinfoptr(),
			       tobjrepo::create("repo.tst"));
	}

	// Fake peer status.

	{
		nodeclusterstatus fakestatus(peername, peeruuid, 0, false);

		peer->peerstatusupdate(fakestatus);
		peer->thisstatus=fakestatus;
	}

	auto slave=x::ref<repocontrollerslaveObj>
		::create(peername, peer,
			 peeruuid,
			 dstrepo,
			 quorum_callback_list,
			 x::ptr<trandistributorObj>(),
			 tracker->getTracker(),
			 x::ref<dummyhalt>::create());

	auto slave_ret = slave->start_controller(x::ptr<x::obj>::create());

	std::cout << "Started slave controller" << std::endl;

	objrepocopysrcptr mastersrc(objrepocopysrcptr::create());

	auto masterdeser=
		x::ref<test3deser>::create(mastersrc, peername, peeruuid);

	x::ptr<x::obj> masterdeser_mcguffin(x::ptr<x::obj>::create());

	tracker->start(masterdeser,
		       sock, x::fdinputiter(sock),
		       tracker->getTracker(),
		       masterdeser_mcguffin);

	std::cout << "Started master deserializer" << std::endl;

	{
		std::unique_lock<std::mutex> lock(masterdeser->mutex);

		while (!masterdeser->copy_requested) // [SLAVESYNCREQUEST]
		{
			masterdeser->cond.wait(lock);
		}
		std::cout << "Received copy request from the slave"
			  << std::endl;
	}

	auto masterser=x::ref<test3serObj>::create(masterdeser);

	x::ptr<x::obj> copysrc_mcguffin=x::ptr<x::obj>::create();

	objrepocopysrcObj::copycomplete complete
		=mastersrc->start(srcrepo, masterser,
				  batonptr(), copysrc_mcguffin);

	std::cout << "Started master copy thread, waiting for copy to be completed"
		  << std::endl;

	{
		x::ptr<x::destroyCallbackFlagObj>
			cb(x::ptr<x::destroyCallbackFlagObj>::create());

		copysrc_mcguffin->addOnDestroy(cb);
		copysrc_mcguffin=x::ptr<x::obj>();
		cb->wait();
	}

	if (!complete->success())
		throw EXCEPTION("Copy failure reported");

	std::cout << "Copy complete" << std::endl;

	{
		newtran tr(srcrepo->newtransaction());

		x::fd fd(tr->newobj("obj3"));

		(*fd->getostream()) << "obj3" << std::endl << std::flush;

		fd->close();

		srcrepo->begin_commit(tr->finalize(), x::eventfd::create())
			->commit();
	}

	{
		std::lock_guard<std::mutex> lock(test3_mutex);

		test3_flag=false;
	}

	complete->release();

	mastersrc->wait();
	std::cout << "Source copy thread terminated" << std::endl;

	{
		std::unique_lock<std::mutex> lock(test3_mutex);

		while (!test3_flag)
			test3_cond.wait(lock); // [SLAVEQUORUM]
	}
	std::cout << "Quorum signal received" << std::endl;

	slave->stop();
	slave_ret->wait();

	std::cout << "Slave controller stopped" << std::endl;

	peer->stop();
	wait_mcguffin(peer_mcguffin);
	std::cout << "Peer connection object terminated" << std::endl;

	wait_mcguffin(masterdeser_mcguffin);
	std::cout << "Master thread terminated" << std::endl;

	std::set<std::string> object_names;

	object_names.insert(dstrepo->obj_begin(),
			    dstrepo->obj_end());

	if (object_names.size() != 2 ||
	    object_names.find("obj2") == object_names.end() ||
	    object_names.find("obj3") == object_names.end())
	{
		std::ostringstream o;
		const char *sep="";

		o << "[SLAVEQUORUM] test failed: ";

		for (std::set<std::string>::iterator b=object_names.begin(),
			     e=object_names.end(); b != e; ++b)
		{
			o << sep << *b;
			sep=", ";
		}

		throw EXCEPTION(o.str());
	}
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		x::dir::base::rmrf("repo.tst");
		ALARM(30);

		x::dir::base::rmrf(clusterdir);
		x::dir::base::rmrf(cluster2dir);

		std::cout << "test1" << std::endl;
		test1();

		std::cout << "test2" << std::endl;
		test2();

		x::dir::base::rmrf(clusterdir);
		x::dir::base::rmrf(cluster2dir);

		std::cout << "test3" << std::endl;
		test3();

		x::dir::base::rmrf(clusterdir);
		x::dir::base::rmrf(cluster2dir);
		x::dir::base::rmrf("repo.tst");
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

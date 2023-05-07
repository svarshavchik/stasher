/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"

#include <x/obj.H>
#include <x/options.H>
#include <x/serialize.H>
#include <x/deserialize.H>
#include <x/logger.H>

#include "repopeerconnectionbase.C"
#include "mastersyncinfo.H"
#include "objwriter.H"
#include "writtenobj.H"
#include "boolref.H"
#include "objrepocopydst.H"
#include "objserializer.H"
#include <iterator>
#include <filesystem>

class trandistihave;
class trandistcancel;
class transerializer;

#define repopeerconnection_H

class repopeerconnectionObj: virtual public x::obj {

protected:
	LOG_CLASS_SCOPE;

public:
	std::string peername;

	x::uuid connuuid;

	repopeerconnectionObj(const std::string &fakepeername)
		: peername(fakepeername)
	{
	}

	~repopeerconnectionObj() {}

	virtual void connect_peer(const repopeerconnectionbaseObj::peerlinkptr
				  &)=0;

	virtual void connect_master(const mastersyncinfo &)=0;

	void distribute_peer(const trandistihave &msg)
	{
	}

	void distribute_peer(const trandistcancel &msg)
	{
	}

	//! Distributor: transerializer message

	void distribute_peer(const x::ref<STASHER_NAMESPACE::writtenObj<transerializer> > &msg)

	{
	}

	void baton_master_announce(const std::string &mastername,
				   const x::uuid &masteruuid,
				   const x::uuid &batonuuid,
				   const std::string &newmasterpeer,
				   const x::ptr<x::obj> &mcguffin)
	{
	}

	void baton_master_release(const std::string &mastername,
				 const x::uuid &masteruuid)
	{
	}

	void baton_transfer_request(const batonptr &baton_arg)
	{
	}

	void ping(const x::ptr<x::obj> &mcguffin)
	{
	}

	void installformermasterbaton(const batonptr &batonp)
	{
	}

	void master_quorum_announce(const std::string &mastername_arg,
				    const x::uuid &uuid_arg,
				    const STASHER_NAMESPACE::quorumstate &state)
	{
	}

	void halt_request(const std::string &mastername_arg,
			  const x::uuid &uuid_arg,
			  const x::ptr<x::obj> &mcguffin)
	{
	}
};

LOG_CLASS_INIT(repopeerconnectionObj);

#define DEBUG_DISABLE_MASTER_COMMITS
#include "repocontrollermaster.C"
#include "mastersyncinfo.C"
#include "clusternotifier.C"
#include "repoclusterquorum.C"
#include "repoclusterquorumcallbackbase.C"
#include "repocontrollerbasehandoff.C"
#include "repocontrollerbase.C"
#include "clusterinfo.C"
#include "objrepocopy.C"
#include "trandistributor.C"
#include "trancanceller.C"
#include "baton.C"

static const char clusterdir[]="conftestcluster.dir";
static const char cluster2dir[]="conftestcluster2.dir";

class test1connectionObj: public repopeerconnectionObj {

public:
	test1connectionObj(const std::string &fakepeername)
 : repopeerconnectionObj(fakepeername)
	{
	}

	~test1connectionObj() {}

        std::mutex mutex;
        std::condition_variable cond;

	repopeerconnectionbaseObj::peerlinkptr masterlink;
	mastersyncinfo synchandle;

	void connect_peer(const repopeerconnectionbaseObj::peerlinkptr
			  &masterlinkArg)
		override
	{
		std::unique_lock<std::mutex> lock(mutex);

		if (!masterlink.null()) // [MASTERREJECTPEER]
			LOG_FATAL("Internal error: duplicate peerlink message");

		masterlink=masterlinkArg;
		cond.notify_all();
	}

	void connect_master(const mastersyncinfo &synchandleArg)
		override
	{
		std::unique_lock<std::mutex> lock(mutex);

		if (!synchandle.null()) // [MASTERREJECTPEER]
			LOG_FATAL("Internal error: duplicate peerlink message");

		synchandle=synchandleArg;
		cond.notify_all();
	}
};

class dummyhalt : public x::stoppableObj {

public:
	dummyhalt() {}
	~dummyhalt() {}

	void stop() override {}
};

class mymastercontrollerObj : public repocontrollermasterObj {

public:
	mymastercontrollerObj(const std::string &masternameArg,
			      const x::uuid &masteruuidArg,
			      const tobjrepo &repoArg,
			      const repoclusterquorum &callback_listArg,
			      const x::ptr<trandistributorObj>
			      &distributorArg,
			      const STASHER_NAMESPACE::stoppableThreadTracker &trackerArg)
		: repocontrollermasterObj(masternameArg, masteruuidArg,
					  repoArg, callback_listArg,
					  distributorArg, trackerArg,
					  x::ref<dummyhalt>::create())
	{
	}

	~mymastercontrollerObj() {}

	using repocontrollermasterObj::started;
	using repocontrollermasterObj::quorum;
};

class test12_nodea {

public:

	class quorumcbObj: public STASHER_NAMESPACE::
		repoclusterquorumcallbackbaseObj {

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
			if (inquorum.full != 0 && inquorum.full != 1)
				abort();

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


	STASHER_NAMESPACE::stoppableThreadTrackerImpl tracker;

	std::string mastername;
	x::uuid masteruuid;

	tobjrepo repo;

	clusterinfo cluster;

	repoclusterquorum quorum_callback_list;
	x::ptr<quorumcbObj> quorumcb;

	x::ref<mymastercontrollerObj> master;

	test12_nodea() : tracker(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create()),
			 mastername("nodea"),
			 repo(tobjrepo::create(clusterdir)),
			 cluster(clusterinfo::create(clusterdir, "test",
						     tracker->getTracker(),
						     STASHER_NAMESPACE
						     ::nodeinfomap())),
			 quorum_callback_list(repoclusterquorum::create()),
			 quorumcb(x::ptr<quorumcbObj>::create()),
			 master(x::ref<mymastercontrollerObj>::create
				(mastername, masteruuid, repo,
				 quorum_callback_list,
				 x::ptr<trandistributorObj>(),
				 tracker->getTracker()))
	{
		auto start_info=repocontroller_start_info::create(master);

		quorum_callback_list->install(quorumcb);
		master->initialize(cluster);

		tracker->start_threadmsgdispatcher(master,
				      start_info->new_controller_queue,
				      x::ref<x::obj>::create());

		(void)master->debug_inquorum();

		quorumcb->wait4(true);
	}
};

static void test1()
{
	test12_nodea nodea;

	auto dummypeer=x::ref<test1connectionObj>::create("nodeb");

	nodea.master->peernewmaster(dummypeer,
				    nodeclusterstatus(nodea.mastername,
						      nodea.masteruuid,
						      0,
						      false));

	{
		STASHER_NAMESPACE::nodeinfomap config;

		config["nodeb"]=STASHER_NAMESPACE::nodeinfo();

		nodea.cluster->update(config);
		nodea.quorumcb->wait4(false);
	}

	nodea.master->peernewmaster(dummypeer,
				    nodeclusterstatus(nodea.mastername,
						      nodea.masteruuid,
						      0, false));


	{
		std::unique_lock<std::mutex> lock(dummypeer->mutex);

		while (dummypeer->masterlink.null() ||
		       dummypeer->synchandle.null())
			dummypeer->cond.wait(lock); // [MASTERPEERLINK]

		// [PEERNOTSLAVE]
		dummypeer->synchandle=mastersyncinfo();
		dummypeer->masterlink=repopeerconnectionbaseObj::peerlinkptr();
	}

	nodea.master->peernewmaster(dummypeer,
				    nodeclusterstatus(nodea.mastername,
						      nodea.masteruuid,
						      0, false));
	{
		std::unique_lock<std::mutex> lock(dummypeer->mutex);

		while (dummypeer->masterlink.null() ||
		       dummypeer->synchandle.null())
			dummypeer->cond.wait(lock);

	}

}

//---------------------------------------------------------------------------


class test2connectionObj: public repopeerconnectionObj {

public:
	class copydstObj : public objrepocopydstinterfaceObj {

	public:

		objrepocopydstptr thread;
		tobjrepo repo;

		tobjrepo masterrepo;

		class resultObj : virtual public x::obj {

		public:
			boolref result;

			std::mutex mutex;
			std::condition_variable cond;
			bool done;

			resultObj()
			: result(boolref::create()), done(false)
			{
			}

			~resultObj()
			{

			}

			void destroyed()
			{
				std::unique_lock<std::mutex> lock(mutex);

				done=true;
				cond.notify_all();
			}

			void wait()
			{
				std::unique_lock<std::mutex> lock(mutex);

				while (!done)
					cond.wait(lock);
			}
		};

		x::ref<resultObj> result;

		copydstObj(const tobjrepo &repoArg,
			   const tobjrepo &masterrepoArg)
			: thread(objrepocopydstptr::create()),
			  repo(repoArg),
			  masterrepo(masterrepoArg),
			  result(x::ref<resultObj>::create())
		{
		}

		~copydstObj()
		{
		}

		void start(const objrepocopysrcinterfaceptr &srcArg)

		{
			auto mcguffin=x::ref<x::obj>::create();

			auto r=result;

			mcguffin->ondestroy([r]{r->destroyed();});

			thread->start(repo, srcArg, result->result,
				      batonptr(), mcguffin);
		}


		void event(const objrepocopy::batonrequest &msg)
			override
		{
			thread->event(msg);
		}

		void event(const objrepocopy::masterlist &msg)
			override
		{

			{
				newtran tr(masterrepo->newtransaction());

				x::fd fd(tr->newobj("obj2"));

				(*fd->getostream()) << "obj2" << std::endl
						    << std::flush;

				fd->close();

				masterrepo->begin_commit(tr->finalize(),
							 x::eventfd::create())
					->commit();
			}

			thread->event(msg);
		}

		void event(const objrepocopy::masterlistdone &msg)
			override
		{
			thread->event(msg);
		}

		void event(const objrepocopy::slaveliststart &msg)
			override
		{
			thread->event(msg);
		}

		void event(const objrepocopy::masterack &msg)
			override
		{
			thread->event(msg);
		}

		void event(const objrepocopy::copycomplete &msg)
			override
		{
			thread->event(msg);
		}

		void event(const objserializer &msg)
			override
		{
			std::vector<char> tmpbuf;

			{
				typedef std::back_insert_iterator
					<std::vector<char> > ins_iter_t;

				ins_iter_t ins_iter(tmpbuf);

				x::serialize
					::iterator<ins_iter_t>
					ser_iter(ins_iter);

				ser_iter(msg);
			}

			{
				typedef std::vector<char>::iterator read_iter;

				read_iter b(tmpbuf.begin()), e(tmpbuf.end());

				x::deserialize
					::iterator<read_iter> deser_iter(b, e);

				objserializer deser(repo, "dummy");

				deser_iter(deser);
			}
		}
	};

	class connectCbObj : public mymastercontrollerObj::syncslave_cbObj {

	public:
		x::weakptr<x::ptr<copydstObj> > copydst;

		connectCbObj(const x::ptr<copydstObj> &copydstArg)
 : copydst(copydstArg)
		{
		}

		~connectCbObj()
		{
		}

		void bind(const objrepocopysrcinterfaceptr &src)
			override
		{
			x::ptr<copydstObj> ptr(copydst.getptr());

			if (!ptr.null())
			{
				std::cout << "Connected src/dst copy interfaces"
					  << std::endl;
				ptr->start(src);
			}
		}
	};

	test2connectionObj(const std::string &fakepeername,
			   const tobjrepo &masterrepo)
		: repopeerconnectionObj(fakepeername),
		  repo(tobjrepo::create(cluster2dir)),
		  copydst(x::ptr<copydstObj>::create(repo, masterrepo))
	{
	}

	~test2connectionObj() {}

	repopeerconnectionbaseObj::peerlinkptr masterlink;
	mastersyncinfo synchandle;
	tobjrepo repo;
	x::ptr<copydstObj> copydst;

	void connect_peer(const repopeerconnectionbaseObj::peerlinkptr
			  &masterlinkArg)
		override
	{
		if (!masterlink.null())
			LOG_FATAL("Internal error: duplicate peerlink message");

		masterlink=masterlinkArg;
	}

	void connect_master(const mastersyncinfo &synchandleArg)
		override
	{
		if (!synchandle.null())
			LOG_FATAL("Internal error: duplicate peerlink message");

		std::cout << "Connecting to master controller" << std::endl;

		synchandle=synchandleArg;

		x::ptr<mymastercontrollerObj>
			controller(synchandle->controller.getptr());

		if (controller.null())
			LOG_FATAL("Internal error: controller disappeared");

		auto cb=x::ref<connectCbObj>::create(copydst);

		controller->accept(repopeerconnectionptr(this),
				   synchandle->connection, masterlink);

		controller->syncslave(connuuid,
				      copydst, peername, batonptr(),
				      cb);
	}
};

static void test2()
{
	test12_nodea nodea;

	{
		newtran tr(nodea.repo->newtransaction());

		x::fd fd(tr->newobj("obj1"));

		(*fd->getostream()) << "obj1" << std::endl << std::flush;

		fd->close();

		nodea.repo->begin_commit(tr->finalize(), x::eventfd::create())
			->commit();
	}


	{
		STASHER_NAMESPACE::nodeinfomap config;

		config["nodeb"]=STASHER_NAMESPACE::nodeinfo();

		nodea.cluster->update(config);
	}

	nodea.quorumcb->wait4(false);

	x::ptr<test2connectionObj>
		conn(x::ptr<test2connectionObj>::create("nodeb",
							nodea.repo));

	nodea.master->peernewmaster(conn, nodeclusterstatus(nodea.mastername,
							    nodea.masteruuid,
							    0,
							    false));
	conn->copydst->result->wait();
	if (!conn->copydst->result->result->flag)
		throw EXCEPTION("Copy thread failed");

	tobjrepoObj::values_t values;

	{
		std::set<std::string> objects, notfound;

		objects.insert("obj1");
		objects.insert("obj2");

		conn->repo->values(objects, true, values, notfound);
	}

	tobjrepoObj::values_t::iterator p;

	if ((p=values.find("obj1")) == values.end())
		throw EXCEPTION("Repository copy failed (obj1 missing)");

	std::string l;

	std::getline(*p->second.second->getistream(), l);

	if (l != "obj1")
		throw EXCEPTION("Repository copy sanity check failed (obj1)");


	if ((p=values.find("obj2")) == values.end())
		throw EXCEPTION("Repository copy failed (obj2 missing)");

	std::getline(*p->second.second->getistream(), l);

	if (l != "obj2")
		throw EXCEPTION("Repository copy sanity check failed (obj2)");

	nodea.quorumcb->wait4(true);

	{
		auto master2=x::ref<mymastercontrollerObj>
			::create(nodea.mastername, nodea.masteruuid, nodea.repo,
				 nodea.quorum_callback_list,
				 x::ptr<trandistributorObj>(),
				 nodea.tracker->getTracker());

		nodea.quorumcb->wait4(false); // [NEWCONTROLLERQUORUMFALSE]

		master2->quorum(STASHER_NAMESPACE::quorumstate(true, true));

		nodea.quorumcb->wait4(false); // [CONTROLLERQUORUMFORCEFALSE]

		master2->started();

		master2->quorum(STASHER_NAMESPACE::quorumstate(true, true));

		nodea.quorumcb->wait4(true); // [CONTROLLERQUORUMRESUME]
	}

	{
		auto master2=x::ref<mymastercontrollerObj>
			::create(nodea.mastername, nodea.masteruuid, nodea.repo,
				 nodea.quorum_callback_list,
				 x::ptr<trandistributorObj>(),
				 nodea.tracker->getTracker());

		nodea.quorumcb->wait4(false);
	}

	nodea.master->quorum(STASHER_NAMESPACE::quorumstate(true, true));
	nodea.quorumcb->wait4(true);
}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		ALARM(30);

		std::filesystem::remove_all(clusterdir);
		std::filesystem::remove_all(cluster2dir);

		std::cout << "test1" << std::endl;

		test1();

		std::filesystem::remove_all(clusterdir);
		std::filesystem::remove_all(cluster2dir);

		std::cout << "test2" << std::endl;

		test2();

		std::filesystem::remove_all(clusterdir);
		std::filesystem::remove_all(cluster2dir);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

/*
** Copyright 2012-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"

#include <x/ref.H>
#include <x/obj.H>
#include <x/exception.H>
#include <list>
#include "trandistihave.H"
#include "trandistcancel.H"
#include "transerializer.H"
#include "stoppablethreadtracker.H"
#include "objwriter.H"
#include "writtenobj.H"

#include "clusternotifierfwd.H"

#define repopeerconnection_H

class repopeerconnectionObj : virtual public x::obj {

public:
	std::string peername;

	repopeerconnectionObj(const std::string &peernameArg)
		: peername(peernameArg)
	{
	}

	~repopeerconnectionObj() noexcept
	{
	}

	std::mutex mutex;
	std::condition_variable cond;

	std::list<trandistihave> ihave_list;
	std::list<trandistcancel> cancel_list;
	std::list<x::uuid> serializer_list;

	void distribute_peer(const trandistihave &msg)
	{
		std::unique_lock<std::mutex> lock(mutex);

		ihave_list.push_back(msg);
		cond.notify_all();
	}

	void distribute_peer(const trandistcancel &msg)
	{
		std::unique_lock<std::mutex> lock(mutex);

		cancel_list.push_back(msg);
		cond.notify_all();
	}

	void distribute_peer(const x::ref<STASHER_NAMESPACE::writtenObj<transerializer> > &msg)

	{
		std::unique_lock<std::mutex> lock(mutex);

		serializer_list.push_back(msg->msg.uuid);
		cond.notify_all();
	}

	trandistihave wait_ihave()
	{
		std::unique_lock<std::mutex> lock(mutex);

		while (ihave_list.empty())
			cond.wait(lock);

		trandistihave msg(ihave_list.front());

		ihave_list.pop_front();
		return msg;
	}

	trandistcancel wait_cancel()
	{
		std::unique_lock<std::mutex> lock(mutex);

		while (cancel_list.empty())
			cond.wait(lock);

		trandistcancel msg(cancel_list.front());

		cancel_list.pop_front();
		return msg;
	}

	x::uuid wait_tran()
	{
		std::unique_lock<std::mutex> lock(mutex);

		while (serializer_list.empty())
			cond.wait(lock);

		x::uuid msg=serializer_list.front();

		serializer_list.pop_front();
		return msg;
	}

};

#include "nodeinfoconn.H"

#define repoclusterinfo_H

class clusternotifierObj;

class repoclusterinfoObj : virtual public x::obj {

public:

	x::weakptr<x::ptr<clusternotifierObj> > notifier;

	typedef std::map<std::string, nodeinfoconn> cluster_t;

	cluster_t cluster;

	std::string peername;

	std::string getthisnodename() noexcept
	{
		return peername;
	}

	void installnotifycluster(const clusternotifier &notifier)
;

	repoclusterinfoObj(const std::string &peername,
			   const cluster_t &cluster);

	~repoclusterinfoObj() noexcept;

	void renotify(const cluster_t &clusterArg);
};

#include "trandistributor.C"
#include "trancanceller.C"
#include <x/options.H>


repoclusterinfoObj::repoclusterinfoObj(const std::string &peernameArg,
				       const cluster_t &clusterArg)
 : cluster(clusterArg), peername(peernameArg)
{
}

repoclusterinfoObj::~repoclusterinfoObj() noexcept
{
}

void repoclusterinfoObj::installnotifycluster(const clusternotifier
					      &notifierArg)

{
	notifier=notifierArg;
	renotify(cluster);
}


void repoclusterinfoObj::renotify(const cluster_t &clusterArg)
{
	x::ptr<clusternotifierObj> n=notifier.getptr();

	if (!n.null())
		n->clusterupdated(clusterArg);
}


static const char repo1[]="conftest1.dir";

static void test1()
{
	tobjrepo repo(tobjrepo::create(repo1));

	repoclusterinfoObj::cluster_t clustermap;

	clustermap["nodea"]=nodeinfoconn();
	clustermap["nodeb"]=nodeinfoconn();

	auto cluster=repoclusterinfo::create("nodea", clustermap);

	STASHER_NAMESPACE::stoppableThreadTrackerImpl
		tracker(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create());

	x::ref<trandistributorObj> distributor(x::ref<trandistributorObj>
					       ::create());

	x::uuid trannodea( ({
				newtran tr(repo->newtransaction());

				tr->getOptions()
					[ tobjrepoObj::node_opt ]
					= "nodea";
				tr->finalize();
			}));

	x::uuid trannodea2( ({
				newtran tr(repo->newtransaction());

				tr->getOptions()
					[ tobjrepoObj::node_opt ]
					= "nodea";
				tr->finalize();
			}));

	x::uuid trannodeb( ({
				newtran tr(repo->newtransaction());

				tr->getOptions()
					[ tobjrepoObj::node_opt ]
					= "nodeb";
				tr->finalize();
			}));

	tracker->start_thread(distributor,
			      trandistributorObj::msgqueue_obj::create(distributor),
			      cluster, repo,
			      x::ptr<x::obj>::create());

	auto conn=repopeerconnection::create("nodeb");

	distributor->connected("nodeb", repopeerconnectionptr());

	distributor->connected("nodeb", conn);

	{
		trandistihave ihave(conn->wait_ihave());

		// [TRANDISTIHAVE]

		if (ihave.uuids.size() != 1 ||
		    ihave.uuids.find(trannodeb) == ihave.uuids.end())
			throw EXCEPTION("[TRANDISTIHAVE] failed");
	}

	x::uuid trannodea3;

	{
		trandistihave ihave;

		ihave.uuids.insert(trannodea2);
		ihave.uuids.insert(trannodea3);

		distributor->deserialized(ihave, repopeerconnectionptr()
					  );
		distributor->deserialized(ihave, conn);
	}

	{
		x::uuid serializer=conn->wait_tran();
		trandistcancel cancel(conn->wait_cancel());

		// [TRANDISTSYNC]
		if (serializer != trannodea ||
		    cancel.uuids.size() != 1 ||
		    cancel.uuids.find(trannodea3) == cancel.uuids.end())
			throw EXCEPTION("[TRANDISTSYNC] failed");
	}

	{
		newtran tr(repo->newtransaction());

		tr->getOptions()[ tobjrepoObj::node_opt ]="nodea";

		distributor->newtransaction(tr, x::ptr<x::obj>());
	}

	trannodea3=conn->wait_tran(); // [TRANDISTNEW]

	distributor->canceltransaction(trannodea3);

	{
		trandistcancel cancel(conn->wait_cancel());

		if (cancel.uuids.size() != 1 ||
		    cancel.uuids.find(trannodea3) == cancel.uuids.end())
			// [TRANDISTCANCEL]
			throw EXCEPTION("[TRANDISTCANCEL] failed");
	}
}

class test2_receiverObj : public trandistreceivedObj {

public:

	std::mutex mutex;
	std::condition_variable cond;

	trandistuuid uuids;

	test2_receiverObj()
		: uuids(trandistuuid::create()) {}
	~test2_receiverObj() noexcept {}

	void received(const trandistuuid &recvuuids)

	{
		std::unique_lock<std::mutex> lock(mutex);

		uuids->received(recvuuids);

		cond.notify_all();
	}

	void cancelled(const tranuuid &cancuuids)

	{
		std::unique_lock<std::mutex> lock(mutex);

		uuids->cancelled(cancuuids);

		cond.notify_all();
	}
};

typedef x::ptr<test2_receiverObj> test2_receiver;

static void test2()
{
	tobjrepo repo(tobjrepo::create(repo1));

	repoclusterinfoObj::cluster_t clustermap;

	clustermap["nodea"]=nodeinfoconn();
	clustermap["nodeb"]=nodeinfoconn();

	auto cluster=repoclusterinfo::create("nodea", clustermap);

	STASHER_NAMESPACE::stoppableThreadTrackerImpl
		tracker(STASHER_NAMESPACE::stoppableThreadTrackerImpl::create());

	x::ref<trandistributorObj>
		distributor(x::ref<trandistributorObj>::create());

	tracker->start_thread(distributor,
			      trandistributorObj::msgqueue_obj::create(distributor),
			      cluster, repo, x::ptr<x::obj>());

	x::uuid trannodea;

	distributor->newtransaction(repo->newtransaction(trannodea),
				    x::ptr<x::obj>());

	x::uuid trannodec;

	{
		newtran tr(repo->newtransaction(trannodec));

		tr->getOptions()[ tobjrepoObj::node_opt ]= "nodec";

		transerializer serializer(repo,spacemonitor
					  ::create(x::df::create(".")));

		serializer.uuid=trannodec;
		serializer.tran=tr;

		distributor->deserialized(serializer);
	}

	x::uuid trannodeb;

	distributor->deserialized_fail(trannodeb,
				       dist_received_status_t
				       (dist_received_status_err, "nodeb"));

	test2_receiver receiver(test2_receiver::create());

	distributor->installreceiver(receiver);

	{
		std::unique_lock<std::mutex> lock(receiver->mutex);

		while (receiver->uuids->uuids.find(trannodea)
		       == receiver->uuids->uuids.end()
		       &&
		       receiver->uuids->uuids.find(trannodeb)
		       == receiver->uuids->uuids.end())
			// [TRANFAILEDANNOUNCERECV]
			receiver->cond.wait(lock);

		if (receiver->uuids->uuids.find(trannodec)
		    != receiver->uuids->uuids.end()) // [TRANSOURCEUNKNOWN]
			throw EXCEPTION("[TRANSOURCEUNKNOWN] failed");
	}

	distributor->canceltransaction(trannodea);

	{
		std::unique_lock<std::mutex> lock(receiver->mutex);

		while (receiver->uuids->uuids.find(trannodea)
		       != receiver->uuids->uuids.end())
			receiver->cond.wait(lock);
	}

	{
		std::pair<objrepoObj::tmp_iter_t,
			  objrepoObj::tmp_iter_t> tmps=
			objrepo(x::ptr<x::obj>(repo))->tmp_iter();

		std::list<std::string> n;

		std::copy(tmps.first, tmps.second,
			  std::back_insert_iterator<std::list<std::string> >
			  (n));

		if (!n.empty())
			throw EXCEPTION("test2 failed");
	}

	auto conn=repopeerconnection::create("nodeb");

	distributor->connected("nodeb", conn); // [TRANFAILEDIHAVE]

	{
		trandistihave ihave(conn->wait_ihave());

		if (ihave.uuids.size() != 1 ||
		    ihave.uuids.find(trannodeb) == ihave.uuids.end())
			throw EXCEPTION("[TRANFAILEDIHAVE] failed");
	}

	clustermap.erase("nodeb");
	cluster->renotify(clustermap);

	{
		std::unique_lock<std::mutex> lock(receiver->mutex);

		while (receiver->uuids->uuids.find(trannodeb)
		       != receiver->uuids->uuids.end())
			// [TRANFAILEDREMOVERECV]
			receiver->cond.wait(lock);
	}

}

int main(int argc, char **argv)
{
#include "opts.parse.inc.tst.C"

	try {
		x::dir::base::rmrf(repo1);

		ALARM(60);

		std::cout << "test1" << std::endl;
		test1();

		std::cout << "test2" << std::endl;
		x::dir::base::rmrf(repo1);
		test2();

		x::dir::base::rmrf(repo1);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}
	return 0;
}

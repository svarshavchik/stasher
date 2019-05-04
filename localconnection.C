/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "localconnection.H"
#include "trandistributor.H"
#include "repoclusterinfoimpl.H"
#include "stasher/userhelo.H"
#include "usergetuuidsreply.H"
#include "userput_deserialized.H"
#include "userchanged.H"
#include "stasher/repoclusterquorumcallbackbase.H"
#include "deserobjname.H"
#include "trancommit.H"
#include "stasher/objname.H"
#include <x/hier.H>
#include <x/strtok.H>
#include "x/join.H"
#include <cstdio>
#include <cstring>
MAINLOOP_IMPL(localconnectionObj)

LOG_CLASS_INIT(localconnectionObj);

x::property::value<size_t>
localconnectionObj::maxgets_prop("connection::maxgets", 10);

x::property::value<size_t>
localconnectionObj::writequeuesize_prop("connection::writequeuesize", 100);

x::property::value<size_t>
localconnectionObj::diritersize_prop("connection::diritersize", 10);

x::property::value<size_t>
localconnectionObj::maxsubs_prop("connection::maxsubs", 10);

class localconnectionObj::quorumstatusObj
	: public STASHER_NAMESPACE::repoclusterquorumcallbackbaseObj {

	x::weakptr<x::ptr<localconnectionObj> > conn;

public:

	quorumstatusObj(const x::ptr<localconnectionObj> &connArg)
		: conn(connArg)
	{
	}

	~quorumstatusObj()
	{
	}

	void quorum(const STASHER_NAMESPACE::quorumstate &state) override
	{
		x::ptr<localconnectionObj> ptr=conn.getptr();

		if (!ptr.null())
			ptr->quorumstatuschanged(state);
	}
};

localconnectionObj::localconnectionObj(const std::string &threadnameArg,
				       const nsview &namespaceviewArg,
				       const tobjrepo &repoArg,
				       const STASHER_NAMESPACE::stoppableThreadTracker &trackerArg,
				       const x::ptr<trandistributorObj>
				       &distributorArg,
				       const repoclusterinfoimpl
				       &clusterArg,
				       const spacemonitor
				       &spacemonitorArg,
				       const x::semaphore &getsemaphoreArg)

	: fdobjrwthreadObj("write(" + threadnameArg + ")"), name(threadnameArg),
	  repo(repoArg), namespaceview(namespaceviewArg),
	  tracker(trackerArg), distributor(distributorArg),
	  cluster(clusterArg), spacedf(spacemonitorArg),
	  getsemaphore(getsemaphoreArg),
	  limits(maxgets_prop.get(),
		 clusterArg->getmaxobjects(),
		 clusterArg->getmaxobjectsize(),
		 x::fdbaseObj::get_buffer_size(),
		 maxsubs_prop.get())
{
	readTimeout_value=0;
}

localconnectionObj::~localconnectionObj()
{
}

class localconnectionObj::getinfo {

public:

	x::uuid requuid;

	std::set<std::string> objects;

	std::map<std::string, std::string> namespacemap;

	bool openobjects;

	bool admin;

	tobjrepoObj::lockentryptr_t lock;

	// Before we can proceed with a get, if it wants to open the objects
	// we need to allocate the requisite number of semaphores, in order
	// to control the maximum number of open file descriptors.

	class getsemaphoreObj : public x::semaphore::base::ownerObj {

		x::ptr<x::obj> mcguffin;

		x::eventfd eventfd;

	public:
		getsemaphoreObj(const x::eventfd &eventfdArg)
			: eventfd(eventfdArg)
		{
		}

		~getsemaphoreObj()
		{
		}

		void process(const x::ref<x::obj> &mcguffinArg) override
		{
			std::lock_guard<std::mutex> lock(objmutex);

			mcguffin=mcguffinArg;

			eventfd->event(1);
		}

		x::ptr<x::obj> getMcguffin()
		{
			std::lock_guard<std::mutex> lock(objmutex);

			return mcguffin;
		}
	};

	x::ptr<getsemaphoreObj> semaphore;

	getinfo(const x::uuid &requuidArg) : requuid(requuidArg)
	{
	}
};


void localconnectionObj::drain()
{
	fdobjrwthreadObj::drain();

	std::list<getinfo>::iterator b=getqueue->begin(),
		e=getqueue->end(), p;

	if (b == e)
		return;

	while ((p=b) != e)
	{
		++b;

		x::ptr<x::obj> semaphore_mcguffin;

		if (!p->lock.null() && p->lock->locked()
		    && (p->semaphore.null() ||
			!(semaphore_mcguffin=p->semaphore->getMcguffin())
			.null()))
		{
			x::ref<STASHER_NAMESPACE
			       ::writtenObj<STASHER_NAMESPACE
					    ::usergetuuidsreply> > ack=
				x::ref<STASHER_NAMESPACE
				       ::writtenObj<STASHER_NAMESPACE
						    ::usergetuuidsreply> >
				::create(p->requuid);

			ack->msg.semaphore_mcguffin=semaphore_mcguffin;

			getready(*p, ack->msg);
			writer->write(ack);
			getqueue->erase(p);
			--getqueue_cnt;
		}
	}
}

class localconnectionObj::userchangedmsgObj
	: public STASHER_NAMESPACE::writtenobjbaseObj {

public:
	STASHER_NAMESPACE::userchanged msg;

	userchangedmsgObj(const std::string &objname,
			  const std::string &suffix)
		: msg(objname, suffix)
	{
	}

	~userchangedmsgObj()
	{
	}

	void serialize(STASHER_NAMESPACE::objwriterObj &writer) override
	{
		writer.serialize( msg );
	}
};

class localconnectionObj::subscriptionsObj : public objrepoObj::notifierObj {

	x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj> writer;

	// Subscribed objects, keyed by repository object name, value is the
	// client's name in the local namespace view.

	typedef std::multimap<std::string, std::string> revmap_t;

	revmap_t revmap;

	// Individually subscribed objects.
	typedef	std::map<std::string, revmap_t::iterator> subscriber_t;

	subscriber_t subscribed;

	// Whether the name represents a hierarchy.

	// If it's "", or ends with a /, it is.
	static std::pair<bool, std::list<std::string>>
		ishier(const std::string &h)
	{
		std::pair<bool, std::list<std::string>> ret;

		ret.first=true;

		if (h.empty())
			return ret;

		if (*--h.end() == '/')
		{
			x::strtok_str(h.substr(0, h.size()-1), "/", ret.second);
			return ret;
		}
		ret.first=false;
		return ret;
	}

	// Subscriptions to a hierarchy

	// Subscriptions to a hierarchy also get inserted into both subscribed
	// and revmap. They're specified by a name ending in a /. They also
	// get inserted into an x::hier, keyed by the hierarchy
	// itself. Due to namespace views, the same repository hierarchy
	// may be accessible by multiple client paths. This is the value in
	// the x::hier container, which contains a simple counter of how
	// many views map to this repository hierarchy.

	class hiersubObj : virtual public x::obj {

	public:
		size_t counter;

		// We get created by the first sub.

		hiersubObj() : counter(1)
		{
		}

		~hiersubObj()
		{
		}
	};

	typedef x::hier<std::string, x::ref<hiersubObj>> subscribed_hier_t;

	subscribed_hier_t subscribed_hier;
public:

	size_t cnt;

	subscriptionsObj(const x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj>
			 &writerArg)
		: writer(writerArg),
		  subscribed_hier(subscribed_hier_t::create()),
		  cnt(0)
	{
	}

	~subscriptionsObj()
	{
	}

	bool addsub(const std::string &name,
		    const std::string &mapped_name)
	{
		std::lock_guard<std::mutex> lock(objmutex);

		revmap_t::iterator rev_iter=
			revmap.insert(std::make_pair(mapped_name, name));

		if (!subscribed.insert(std::make_pair(name, rev_iter)).second)
		{
			revmap.erase(rev_iter);
			return false;
		}

		auto what=ishier(mapped_name);

		if (what.first)
		{
			subscribed_hier
				->insert([]
					 {
#ifdef TEST_SUBSCRIBED_HIER_CREATED
						 TEST_SUBSCRIBED_HIER_CREATED();
#endif
						 return x::ref<hiersubObj>
							 ::create();
					 },
					 what.second,
					 []
					 (const x::ref<hiersubObj> &existing)
					 {
						 ++existing->counter;
						 return false;
					 });
		}

		++cnt;
		LOG_TRACE("Added subscription for \""
			  << name
			  << "\" (mapped as \""
			  << mapped_name
			  << "\")");
		return true;
	}

	void delsub(const std::string &name)
	{
		std::lock_guard<std::mutex> lock(objmutex);

		subscriber_t::iterator iter=subscribed.find(name);

		if (iter != subscribed.end())
		{
			// This is the object repository name
			LOG_DEBUG("Removing subscription for "
				  << name
				  << " (" << iter->second->first << ")");

			auto what=ishier(iter->second->first);

			if (what.first)
			{
				auto lock=subscribed_hier->create_writelock();

				if (!lock->to_child(what.second, false))
				{
#ifdef TEST_SUBSCRIBED_HIER_EXCEPTION
					TEST_SUBSCRIBED_HIER_EXCEPTION();
#endif

					throw EXCEPTION("Internal error, cannot locate existing hierarchy subscription for " << name);
				}
				if (--lock->entry()->counter == 0)
				{
#ifdef TEST_SUBSCRIBED_HIER_DESTROYED
					TEST_SUBSCRIBED_HIER_DESTROYED();
#endif

					lock->erase();
				}
			}

			revmap.erase(iter->second);
			subscribed.erase(iter);
			--cnt;
			LOG_TRACE("Removed subscription for \""
				  << name << "\"");
		}
	}

	void report(std::ostream &o)
	{
		if (revmap.empty())
			return;

		o << std::endl;
		o << "Subscribed to:" << std::endl;

		for (auto &e:revmap)
		{
			o << "    " << e.first << std::endl;
		}
	}

private:
	// notifier callback
	void installed(const std::string &objname,
		       const x::ptr<x::obj> &lock) override
	{
		notify(objname);
	}

	// notifier
	void removed(const std::string &objname,
		     const x::ptr<x::obj> &lock) override
	{
		notify(objname);
	}

	// notify of an object change

	void notify(const std::string &objname)
	{
		{
			const char *p=objname.c_str();

			if (strncmp(p, tobjrepoObj::done_hier,
				   tobjrepoObj::done_hier_l) == 0 &&
			    p[tobjrepoObj::done_hier_l] == '/')
				return; // Shaddup
		}

		std::lock_guard<std::mutex> lock(objmutex);

		sendupdate(objname, "");

		std::list<std::string> key;

		x::strtok_str(objname, "/", key);

		if (!key.empty())
			key.pop_back();

		auto readlock=subscribed_hier->search(key);

		do
		{
			if (!readlock->exists())
				continue;

			std::string hiername=x::join(readlock->name(), "/");
			size_t hiername_l=hiername.size();

			if (hiername_l)
				++hiername_l;

			sendupdate(objname.substr(0, hiername_l),
				   objname.substr(hiername_l));
		} while (readlock->to_parent());
	}

	// Send the client an update

	void sendupdate(const std::string &objname,
			const std::string &suffix)
	{
		LOG_TRACE("Update: " << objname << ", suffix: " << suffix);

		for (std::pair<revmap_t::iterator,
			       revmap_t::iterator> iters
			     =revmap.equal_range(objname);
		     iters.first != iters.second; ++iters.first)
		{
			writer->write(x::ref<userchangedmsgObj>
				      ::create(iters.first->second,
					       suffix));
		}
	}
};

void localconnectionObj::run(//! threadmsgdispatcher mcguffin,
			     x::ptr<x::obj> &threadmsgdispatcher_mcguffin,

			     //! The file descriptor
			     x::fd &transport,

			     //! The input iterator for the file descriptor
			     const x::fd::base::inputiter &inputiter,

			     //! Thread tracker that the writer thread gets registered with
			     const STASHER_NAMESPACE::stoppableThreadTracker
			     &tracker,

			     //! The fdobjrwthreadObj mcguffin
			     const x::ptr<x::obj> &mcguffin)
{
	msgqueue_auto msgqueue(this);
	threadmsgdispatcher_mcguffin=nullptr;

	try {
		socket= &transport;
		readTimeout_value=0; // No effective read timeout

		x::ptr<STASHER_NAMESPACE::userput::deserializedObj> newtranref;
		newtran= &newtranref;

		std::list<getinfo> getqueueref;
		getqueue_cnt=0;

		getqueue= &getqueueref;

		auto quorumstatusref=x::ref<quorumstatusObj>
			::create(x::ptr<localconnectionObj>(this));

		currentstate=STASHER_NAMESPACE::quorumstate();
		wantupdates=false;
		stat_quorum_received=false;
		stat_state_received=false;
		stat_cluster_received=false;
		cluster->installQuorumNotification(quorumstatusref);

		x::ptr<subscriptionsObj> subscriptionsref;
		subscriptions= &subscriptionsref;

		mainloop(msgqueue, transport, inputiter, tracker, mcguffin);
	} catch (const x::stopexception &e)
	{
	} catch (const x::exception &e)
	{
		LOG_ERROR(e);
	}
}

void localconnectionObj::started()
{
	writer->setmaxqueuesize(writequeuesize_prop.get());

	typedef x::ref<STASHER_NAMESPACE::writtenObj
		       <STASHER_NAMESPACE::userhelo> > hello_msg_t;

	hello_msg_t hello(hello_msg_t::create(limits));

	hello->msg.nodename=cluster->getthisnodename();
	hello->msg.clustername=cluster->getthisclustername();
	writer->write(hello);

	auto newsubscriptions=
		x::ref<subscriptionsObj>::create
		(x::ref<STASHER_NAMESPACE::fdobjwriterthreadObj>
		 (writer));

	*subscriptions=newsubscriptions;
	subscriptionsptr= &*newsubscriptions;

	repo->installNotifier(*subscriptions);
}

// Callback -- once a transaction completes, punt it back where it came from

// This gets attached as a destructor callback on the transaction's mcguffin.
//
class localconnectionObj::put_cb : virtual public x::obj {

public:
	x::weakptr<x::ptr<localconnectionObj> > conn;

	x::uuid requuid;

	trandistributorObj::transtatus stat;

	put_cb(const x::ptr<localconnectionObj> &connArg,
	       const x::uuid &requuidArg,
	       const trandistributorObj::transtatus &statArg)
		: conn(connArg), requuid(requuidArg), stat(statArg)
	{
	}

	~put_cb()
	{
	}

	void destroyed()
	{
		x::ptr<localconnectionObj> ptr=conn.getptr();

		if (!ptr.null())
			ptr->transaction_done(requuid, stat);
	}
};

STASHER_NAMESPACE::puttransactionObj::content_str::content_str(const localconnectionObj &conn)
 : chunksize(conn.limits.chunksize)
{
}

void localconnectionObj::deserialized(const STASHER_NAMESPACE::userput &msg)
{
	*newtran=x::ptr<STASHER_NAMESPACE::userput::deserializedObj>
		::create(msg.tran, limits, spacedf, repo,
			 msg.uuid, namespaceview);

	check_deserialized_transaction();
}

void localconnectionObj::deserialized(const STASHER_NAMESPACE::puttransactionObj::content_str &msg)

{
	if (newtran->null())
		throw EXCEPTION("Unexpected content_str message");
	(*newtran)->received(msg);
	check_deserialized_transaction();
}

void localconnectionObj::accept_fds(size_t n)
{
	if (n != 1)
		throw EXCEPTION("Client wants to send more than one file descriptor");
}

void localconnectionObj::received_fd(const std::vector<x::fdptr> &fdArg)

{
	if (newtran->null())
		throw EXCEPTION("Unexpected file descriptor received");

	(*newtran)->received(fdArg[0]);
	check_deserialized_transaction();
}

void localconnectionObj::check_deserialized_transaction()
{
	if (!(*newtran)->completed())
		return;

	x::ref<STASHER_NAMESPACE::userput::deserializedObj> deser= *newtran;
	*newtran=x::ptr<STASHER_NAMESPACE::userput::deserializedObj>();

	if (deser->errcode != STASHER_NAMESPACE::req_processed_stat)
	{
		done(deser->requuid, deser->errcode, deser->requuid);
		return;
	}

	if (deser->puttran->admin)
	{
		x::uuid uuid=deser->tran->finalize();

		deser->tran->getOptions()[tobjrepoObj::node_opt]=
			cluster->getthisnodename();

		trancommit commit=
			deser->repo->begin_commit(uuid,
						  get_msgqueue()->get_eventfd());

		while (!commit->ready())
			wait_eventqueue(-1);

		commit->commit();

		done(deser->requuid, STASHER_NAMESPACE::req_processed_stat,
		     uuid);
		return;
	}

	x::ptr<x::obj> mcguffin=x::ptr<x::obj>::create();

	trandistributorObj::transtatus
		stat=distributor->newtransaction(deser->tran, mcguffin);

	x::ref<put_cb> cb=x::ref<put_cb>
		::create(x::ptr<localconnectionObj>(this), deser->requuid,
			 stat);

	deser->diskspace->commit();
	mcguffin->ondestroy([cb]{cb->destroyed();});
}

bool localconnectionObj::allow_admin_get()

{
	return false;
}

void localconnectionObj::dispatch_transaction_done(const x::uuid &uuid,
						   const trandistributorObj::transtatus &status)
{
	done(uuid, status->status, status->uuid);
}

void localconnectionObj::done(const x::uuid &uuidArg,
			      STASHER_NAMESPACE::req_stat_t status,
			      const x::uuid &newobjuuidArg)
{
	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<STASHER_NAMESPACE::userputreply> >
		      ::create(STASHER_NAMESPACE::userputreply(uuidArg,
							       status,
							       newobjuuidArg)));
}

void localconnectionObj::deserialized(const STASHER_NAMESPACE::usergetuuids
				      &msg)

{
	getinfo gi(msg.requuid);

	{
		const STASHER_NAMESPACE::usergetuuids::reqinfoObj
			&req=*msg.reqinfo;

		for (std::set<STASHER_NAMESPACE::serobjname>::const_iterator
			     b=req.objects.begin(), e=req.objects.end();
		     b != e; ++b)
		{
			std::string n= *b;

			if (!namespaceview->fromclient_read(n))
			{
				usergetuuids_fail(msg.requuid,
						  "Invalid object name: " + n);

				return;
			}

			if (!gi.namespacemap.insert(std::make_pair(n, *b))
			    .second)
			{
				usergetuuids_fail(msg.requuid,
						  "Object name " + n +
						  " also apparently mapped"
						  " under a different name");
				return;
			}
			gi.objects.insert(n);
		}

		gi.openobjects=req.openobjects;
		gi.admin=req.admin;

		if (!allow_admin_get())
			gi.admin=false;
	}

	check_get_lock(gi);

	if (gi.openobjects)
	{
		gi.semaphore=x::ptr<getinfo::getsemaphoreObj>
			::create(get_msgqueue()->get_eventfd());
		getsemaphore->request(gi.semaphore, gi.objects.size());
	}

	getqueue->push_back(gi);
	if (++getqueue_cnt > limits.maxgets)
		throw EXCEPTION("Client exceeded maximum number of configured GET requests");

#ifdef	DEBUG_TEST_GETQUEUE_SERVER_RECEIVED
	DEBUG_TEST_GETQUEUE_SERVER_RECEIVED();
#endif
}

void localconnectionObj::check_get_lock(getinfo &gi)
{
	if (currentstate.majority || gi.admin)
	{
		if (gi.lock.null())
			gi.lock=repo->lock(gi.objects,
					   get_msgqueue()->get_eventfd());
	}
	else
	{
		// When the cluster is not in quorum, we need to drop all
		// locks, so that the master can sync this slave. When the slave
		// syncs, it acquires lock each batch of objects in the
		// repository.

		gi.lock=tobjrepoObj::lockentryptr_t();

#ifdef DEBUG_LOCALCONNECTION_NOTQUORUM
		DEBUG_LOCALCONNECTION_NOTQUORUM();
#endif
	}
}

void localconnectionObj::usergetuuids_fail(const x::uuid &requuid,
					   const std::string &errmsg)

{
	x::ref<STASHER_NAMESPACE::writtenObj<STASHER_NAMESPACE
					     ::usergetuuidsreply> >
		ack=x::ref<STASHER_NAMESPACE::writtenObj<STASHER_NAMESPACE
							 ::usergetuuidsreply> >
		::create(requuid);

	ack->msg.succeeded=false;
	ack->msg.errmsg=errmsg;
	writer->write(ack);
}

void localconnectionObj::getready(const getinfo &gi,
				  STASHER_NAMESPACE::usergetuuidsreply
				  &ack)

{
	tobjrepoObj::values_t valuesMap;

	try
	{
		std::set<std::string> notfound;

		repo->values(gi.objects, gi.openobjects, valuesMap, notfound);

		std::vector<x::fd> filedescs;

		filedescs.reserve(valuesMap.size());

		for (tobjrepoObj::values_t::const_iterator b=valuesMap.begin(),
			     e=valuesMap.end(); b != e; ++b)
		{
			if (gi.openobjects)
				filedescs.push_back(b->second.second);

			std::map<std::string, std::string>::const_iterator
				ns_iter=gi.namespacemap.find(b->first);

			if (ns_iter == gi.namespacemap.end())
				throw EXCEPTION("Internal error: cannot find "
						"mapping for object name "
						+ b->first);

			ack.uuids->push_back(std::make_pair(ns_iter->second,
							    STASHER_NAMESPACE::
							    retrobj(b->second
								    .first)));

		}

		if (gi.openobjects && filedescs.size() > 0)
		{
			x::ref<STASHER_NAMESPACE::writtenObj<STASHER_NAMESPACE::usergetuuidsreply::openobjects> >
				objfollows=
				x::ref<STASHER_NAMESPACE::writtenObj<STASHER_NAMESPACE::usergetuuidsreply
						  ::openobjects> >
				::create(gi.requuid);

			writer->write(objfollows);
			writer->sendfd(filedescs);
		}

	} catch (const x::exception &e)
	{
		std::ostringstream o;

		o << e;

		ack.succeeded=false;
		ack.errmsg=o.str();
	}
}

void localconnectionObj::deserialized(const STASHER_NAMESPACE::fdobjwriterthreadObj::sendfd_ready
				      &msg)
{
	writer->sendfd_proceed(msg);
}

// ----------------------------------------------------------------------------

class localconnectionObj::senddir
	: public STASHER_NAMESPACE::dirmsgreply::getchunk {

public:
	size_t chunksize;

	tobjrepo repo;

	std::string hier;
	nsview namespaceview;
	std::string mapped_hier;

	bool open;

	tobjrepoObj::dir_iter_t b, e;
	std::string orig_pfix;
	size_t strip_mapped_pfix;

	senddir(size_t chunksizeArg,
		const tobjrepo &repoArg,
		const std::string &hierArg,
		const nsview &namespaceviewArg
		) : chunksize(chunksizeArg),
					repo(repoArg),
					hier(hierArg),
					namespaceview(namespaceviewArg),
					open(true)
	{
	}

	~senddir()
	{
	}

	bool nextchunk(std::set<std::string> &ret) override
	{
		bool opened=open;

		if (open)
		{
			try
			{
				if (hier.size() > 0)
				{
					char buf[repo->obj_name_len(hier)];

					repo->obj_name_create(hier, buf);
				}
			} catch (const x::exception &e)
			{
				status=STASHER_NAMESPACE::req_badname_stat;
				return false;
			}

			mapped_hier=hier;

			if (!namespaceview->fromclient_dir(mapped_hier))
			{
				status=STASHER_NAMESPACE::req_eperm_stat;
				return false;
			}

			e=repo->dir_end();

			try {
				b=repo->dir_begin(mapped_hier);
			} catch (const x::exception &dummy)
			{
				// Probably does not exist, quietly punt

				b=e;
			}
			open=false;

			orig_pfix=hier;

			if (orig_pfix.size()) orig_pfix += "/";

			strip_mapped_pfix = mapped_hier.size();

			if (strip_mapped_pfix)
				++strip_mapped_pfix;
		}

		ret.clear();

		if (opened && hier.size() == 0)
		{
			for (auto view : namespaceview->getView())
				if (view.first.size())
					ret.insert(view.first + "/");
		}

		for (size_t i=0; i<chunksize; ++i)
		{
			if (b == e)
			{
				status=STASHER_NAMESPACE::req_processed_stat;
				return false;
			}

			ret.insert(orig_pfix +
				   b->substr(strip_mapped_pfix));
			++b;
		}
		return true;
	}
};


void localconnectionObj::deserialized(const getdir_req_t &req)

{
	getdir_resp_msg_t resp(req.requuid);
	getdir_resp_msg_t::resp_t &msgres=resp.getmsg();

	std::string hier=req.hier;

	if (hier.size() > 0 && *--hier.end() == '/')
		hier=hier.substr(0, hier.size()-1);

	msgres.serialize_chunk=
		x::ptr<senddir>::create(diritersize_prop.get(),
					repo, req.hier,
					namespaceview);
	resp.write(writer);
}

std::string localconnectionObj::report(std::ostream &rep)
{
	rep << "Namespace:" << std::endl;
	namespaceview->to_string(rep);

	rep << "Receiving transaction: " << x::to_string(!newtran->null())
	    << std::endl
	    << "There are " << getqueue_cnt
	    << " get request(s) pending:" << std::endl;

	for (std::list<getinfo>::iterator b=getqueue->begin(),
		     e=getqueue->end(); b != e; ++b)
	{
		rep << x::to_string(b->requuid) << ":" << std::endl;

		for (std::set<std::string>::iterator ob=b->objects.begin(),
			     oe=b->objects.end(); ob != oe; ++ob)
		{
			rep << "    Object: " << *ob << std::endl;
		}

		rep << "    Retrieve contents: " << x::to_string(b->openobjects)
		    << std::endl
		    << "    Admin: " << x::to_string(b->admin) << std::endl
		    << "    Object lock acquired: " <<
			x::to_string(b->lock->locked()) << std::endl
		    << "    Semaphore acquired: " <<
			(b->semaphore.null() ? "N/A":
			 x::to_string(!b->semaphore->getMcguffin().null()))
		    << std::endl;
	}
	rep << "In quorum: " << x::to_string(currentstate)
	    << std::endl;

	(*subscriptions)->report(rep);

	return "connection(user, " + getName() + ")";
}

void localconnectionObj::deserialized(const beginsub_req_t &msg)

{
	beginsub_resp_msg_t resp(msg.requuid);
	beginsub_resp_msg_t::resp_t &msgres=resp.getmsg();

	std::string mapped_name;
	bool trailing_slash=false;

	try {
		// Validate the name of the object being requested

		// An empty name is ok.
		std::string n=msg.objname;

		mapped_name=n;

		if (n.size() > 0)
		{
			// Remove trailing slash, it's ok

			if (*--n.end() == '/')
			{
				mapped_name=n=n.substr(0, n.size()-1);
				trailing_slash=true;
			}

			(void)STASHER_NAMESPACE
				::encoded_object_name_length(n.begin(),
							     n.end());
			if (trailing_slash)
				mapped_name += "/"; // Put back what was taken.
		}
		else
		{
			// Make believe, hierarchy.
			trailing_slash=true;
		}
	} catch (const x::exception &e)
	{
		msgres.status=STASHER_NAMESPACE::req_badname_stat;
		resp.write(writer);
		return;
	}

	if (subscriptionsptr->cnt >= limits.maxsubs)
	{
		msgres.status=STASHER_NAMESPACE::req_toomany_stat;
		resp.write(writer);
		return;
	}

	// For the purposes of fromclient_read, append "x".

	if (trailing_slash)
		mapped_name += "x";

	if (!namespaceview->fromclient_read(mapped_name))
	{
		msgres.status=STASHER_NAMESPACE::req_eperm_stat;
		resp.write(writer);
		return;
	}

	if (trailing_slash)
	{
		// Remove the "x"

		mapped_name=mapped_name.substr(0, mapped_name.size()-1);
	}

	if (!subscriptionsptr->addsub(msg.objname,
				      mapped_name))
	{
		msgres.status=STASHER_NAMESPACE::req_toomany_stat;
		resp.write(writer);
		return;
	}

	msgres.status=STASHER_NAMESPACE::req_processed_stat;
	resp.write(writer);
}

void localconnectionObj::deserialized(const endsub_req_t &msg)
{
	endsub_resp_msg_t resp(msg.requuid);

	subscriptionsptr->delsub(msg.objname);
	resp.write(writer);
}

void localconnectionObj::deserialized(const STASHER_NAMESPACE::sendupdatesreq
				      &msg)
{
	wantupdates=msg.wantupdates;
	quorumstatuschanged();
}

void localconnectionObj::dispatch_quorumstatuschanged(const STASHER_NAMESPACE::quorumstate &inquorum)
{
	stat_quorum_received=true;
	currentstate=inquorum;

	for (auto &gi:*getqueue)
		check_get_lock(gi);

	quorumstatuschanged();
}

void localconnectionObj
::dispatch_do_statusupdated(const nodeclusterstatus &newStatus)
{
	stat_state_received=true;
	currentstate.master=newStatus.master;
	quorumstatuschanged();
}

void localconnectionObj
::dispatch_do_clusterupdated(const clusterinfoObj::cluster_t &newStatus)
{
	stat_cluster_received=true;
	currentstate.nodes.clear();

	for (auto node : newStatus)
		currentstate.nodes.insert(node.first);
	quorumstatuschanged();
}

void localconnectionObj::quorumstatuschanged()
{
	LOG_DEBUG("quorumstatuschanged: wantupdates="
		  << wantupdates
		  << ", stat_quorum_received="
		  << stat_quorum_received
		  << ", stat_state_received="
		  << stat_state_received
		  << ", stat_cluster_received="
		  << stat_cluster_received);

	if (!wantupdates ||
	    !stat_quorum_received ||
	    !stat_state_received ||
	    !stat_cluster_received)
		return;

	writer->write(x::ref<STASHER_NAMESPACE
			     ::writtenObj<STASHER_NAMESPACE::clusterstate> >
		      ::create(currentstate));
}

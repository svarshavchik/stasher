/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "objrepocopysrcthread.H"
#include "objrepocopydstinterface.H"
#include "objuuidenumerator.H"
#include "objserializer.H"
#include "tobjrepo.H"
#include "baton.H"
#include "threadmgr.H"

#include <x/threads/run.H>
#include <x/join.H>

LOG_CLASS_INIT(objrepocopysrcthreadObj);

#ifdef DEBUG_THIS

struct DEBUG_COPYSRCTHREAD {
	int dispatch_event_batonresponse=0;
	int dispatch_event_slavelist=0;
	int dispatch_ack_slavelist_masterlistdone=0;
	int dispatch_ack_slavelist_masterlist=0;
	int dispatch_ack_slavelist_masterack=0;
	int dispatch_handle_slavelist=0;
	int dispatch_event_slavelistready=0;
	int dispatch_event_slavelistdone=0;
	int event_loop;

	x::eventqueueObj<x::dispatchablebase> *qptr=0;
};

static DEBUG_COPYSRCTHREAD debug_this;

#define ENTERED(x) (++debug_this.x)
#else
#define ENTERED(x) do {} while(0)
#endif

class objrepocopysrcthreadObj::dispatch_lock : public std::lock_guard<std::mutex> {

	const char *function;
public:
	dispatch_lock(std::mutex &mutex,
		      const char *functionArg);
	~dispatch_lock();
};

objrepocopysrcthreadObj::dispatch_lock::dispatch_lock(std::mutex &mutex,
						      const char *functionArg)
	: std::lock_guard<std::mutex>(mutex), function(functionArg)
{
	LOG_DEBUG("Begin dispatch " << function);
}

objrepocopysrcthreadObj::dispatch_lock::~dispatch_lock()
{
	LOG_DEBUG("End dispatch " << function);
}

#define DISPATCH(name) dispatch_lock lock(dispatch_mutex, # name)

class objrepocopysrcthreadObj::uuidenumObj : public objuuidenumeratorObj {

	objrepocopysrcthreadObj *parent;

public:
	uuidenumObj(const tobjrepo &repoArg,
		    objrepocopysrcthreadObj *parentArg);
	~uuidenumObj();

	objuuidlist nextbatch(const std::set<std::string> &objectnames)
		override;
};

objrepocopysrcthreadObj::uuidenumObj::uuidenumObj(const tobjrepo &repoArg,
						  objrepocopysrcthreadObj
						  *parentArg)
 : objuuidenumeratorObj(repoArg),
			      parent(parentArg)
{
}

objrepocopysrcthreadObj::uuidenumObj::~uuidenumObj()
{
}

objuuidlist
objrepocopysrcthreadObj::uuidenumObj::nextbatch(const std::set<std::string>
						&objectnames)

{
	auto msgqueue=parent->get_msgqueue();

	x::eventfd eventfd(msgqueue->get_eventfd());

	tobjrepoObj::lockentry_t lock(repo->lock(objectnames, eventfd));

	while (!lock->locked())
	{
		while (!msgqueue->empty())
			msgqueue->pop()->dispatch();

		eventfd->event();
	}

	return objuuidenumeratorObj::nextbatch(objectnames);
}

class objrepocopysrcthreadObj::notifierObj
	: public tobjrepoObj::notifierObj {

	x::weakptr<objrepocopydstinterfaceptr > dst;
	tobjrepo repo;

public:
	notifierObj(const x::weakptr<objrepocopydstinterfaceptr >
		    &dstArg,
		    const tobjrepo &repoArg);
	~notifierObj();

	void installed(const std::string &objname,
		       const x::ptr<x::obj> &lock) override;

	void removed(const std::string &objname,
		       const x::ptr<x::obj> &lock) override;
};

objrepocopysrcthreadObj::notifierObj
::notifierObj(const x::weakptr<objrepocopydstinterfaceptr > &dstArg,
	      const tobjrepo &repoArg)
	: dst(dstArg), repo(repoArg)
{
}

objrepocopysrcthreadObj::notifierObj::~notifierObj()
{
}

void objrepocopysrcthreadObj::notifierObj::installed(const std::string &objname,
						     const x::ptr<x::obj> &lock)

{
	objserializer ack(repo, objname, lock);

	objrepocopydstinterfaceptr sink(dst.getptr());

	if (!sink.null())
		sink->event(ack);
}

void objrepocopysrcthreadObj::notifierObj::removed(const std::string &objname,
						   const x::ptr<x::obj> &lock)

{
	installed(objname, lock);
}

LOG_CLASS_INIT(objrepocopysrcthreadObj::copycompleteObj::copycompleteObj);

objrepocopysrcthreadObj::copycompleteObj
::copycompleteObj(const objrepocopydstinterfaceptr &dstArg)

	: succeededflag(false),
	  dst(dstArg)
{
}

objrepocopysrcthreadObj::copycompleteObj::~copycompleteObj()
{
	LOG_DEBUG("copycomplete destructor invoked, status: "
		  << succeededflag);

	try {
		if (succeededflag)
			release();
	} catch (const x::exception &e)
	{
		LOG_FATAL(e);
	}
}

void objrepocopysrcthreadObj::copycompleteObj
::setNotifier(const x::ptr<notifierObj> &notifierArg)

{
	std::lock_guard<std::mutex> lock(mutex);

	notifier=notifierArg;
}

void objrepocopysrcthreadObj::copycompleteObj
::setSuccesfull(const batonptr &batonArg)

{
	std::lock_guard<std::mutex> lock(mutex);

	succeededflag=true;
	batonp=batonArg;
}

bool objrepocopysrcthreadObj::copycompleteObj::success()
{
	std::lock_guard<std::mutex> lock(mutex);

	return succeededflag;
}

void objrepocopysrcthreadObj::copycompleteObj::release()
{
	objrepocopydstinterfaceptr sink(dst.getptr());

	notifier=x::ptr<notifierObj>();
	dst=objrepocopydstinterfaceptr();
	batonp=batonptr();

	if (!sink.null())
	{
		objrepocopy::copycomplete ack;

		sink->event(ack);
	}
}

objrepocopysrcthreadObj::objrepocopysrcthreadObj(const std::string &nameArg)
	: name(nameArg)
{
}

objrepocopysrcthreadObj::~objrepocopysrcthreadObj()
{
}

std::string objrepocopysrcthreadObj::getName() const noexcept
{
	return name;
}

objrepocopydstinterface objrepocopysrcthreadObj::getdst()
{
	objrepocopydstinterfaceptr dstref(dst.getptr());

	if (dstref.null())
		throw EXCEPTION("Lost connection to slave");

	return dstref;
}

void objrepocopysrcthreadObj::event(const objrepocopy::batonresponse &msg)

{
	event_batonresponse(msg);
}

void objrepocopysrcthreadObj::event(const objrepocopy::slavelist &msg)

{
	event_slavelist(msg);
}

void objrepocopysrcthreadObj::event(const objrepocopy::slavelistready &msg)

{
	event_slavelistready(msg);
}

void objrepocopysrcthreadObj::event(const objrepocopy::slavelistdone &msg)

{
	event_slavelistdone(msg);
}

void objrepocopysrcthreadObj::dispatch_event_batonresponse(const objrepocopy::batonresponse &msg)
{
	ENTERED(dispatch_event_batonresponse);

	if (!batonp->null() && x::to_string((*batonp)->batonuuid) == msg.uuid)
	{
		LOG_DEBUG("Received valid BATONRESPONSE");

		// Short-circuited

#ifdef DEBUG_BATON_TEST_6_VERIFY
		DEBUG_BATON_TEST_6_VERIFY();
#endif

		dispatch_event_slavelistdone(objrepocopy::slavelistdone());
		return;
	}

	LOG_DEBUG("Ignoring BATONRESPONSE");

	*batonp=batonptr();

	uuidenum=x::ptr<uuidenumObj>::create(*repo, this);

	// Simulate a dummy slavelist response, in order to kick-start the
	// masterlist messages
	objrepocopy::slavelist dummy;

	dummy.uuids=objuuidlist::create();
	dispatch_event_slavelist(dummy);
}

void objrepocopysrcthreadObj::dispatch_event_slavelist(const objrepocopy::slavelist &msg)
{
	ENTERED(dispatch_event_slavelist);

	void (objrepocopysrcthreadObj::*ack_func)(objuuidlist &uuidlist);

	objuuidlist uuid;

	dispatch_handle_slavelist(msg);

	{
		DISPATCH(slavelist);

		if (!uuidenum.null())
		{
			uuid=uuidenum->next();

			if (uuid.null())
			{
				uuidenum=x::ptr<uuidenumObj>();

				ack_func=&objrepocopysrcthreadObj
					::dispatch_ack_slavelist_masterlistdone;
			}
			else
			{
				ack_func=&objrepocopysrcthreadObj
					::dispatch_ack_slavelist_masterlist;
			}
		}
		else
		{
			ack_func=&objrepocopysrcthreadObj
				::dispatch_ack_slavelist_masterack;
		}
	}

	(this->*ack_func)(uuid);
}

void objrepocopysrcthreadObj
::dispatch_ack_slavelist_masterlistdone(objuuidlist &uuidList)
{
	ENTERED(dispatch_ack_slavelist_masterlistdone);

	objrepocopy::masterlistdone ack;

	getdst()->event(ack);
}

void objrepocopysrcthreadObj
::dispatch_ack_slavelist_masterlist(objuuidlist &uuidList)
{
	ENTERED(dispatch_ack_slavelist_masterlist);

	objrepocopy::masterlist ack;

	ack.uuids=uuidList;
	getdst()->event(ack);
}

void objrepocopysrcthreadObj
::dispatch_ack_slavelist_masterack(objuuidlist &uuidList)
{
	ENTERED(dispatch_ack_slavelist_masterack);

	objrepocopy::masterack ack;
	getdst()->event(ack);
}

void objrepocopysrcthreadObj
::dispatch_handle_slavelist(const objrepocopy::slavelist &msg)
{
	ENTERED(dispatch_handle_slavelist);

	auto msgqueue=get_msgqueue();

	std::set<std::string> object_names=msg.uuids->objnouuids;

	tobjrepoObj::values_t values;

	for (std::map<std::string, x::uuid>::const_iterator
		     b(msg.uuids->objuuids.begin()),
		     e(msg.uuids->objuuids.end()); b != e; ++b)
		object_names.insert(b->first);

	tobjrepoObj::lockentryptr_t lock;

	if (!object_names.empty())
	{
		x::eventfd eventfd(msgqueue->get_eventfd());

		LOG_DEBUG("Locking repository for: "
			  << x::join(object_names.begin(),
				     object_names.end(), ", "));

		lock=(*repo)->lock(object_names, eventfd);

		while (!lock->locked())
		{
			while (!msgqueue->empty())
				msgqueue->pop()->dispatch();

			eventfd->event();
		}

		LOG_DEBUG("Locked repository for: "
			  << x::join(object_names.begin(),
				     object_names.end(), ", "));

		std::set<std::string> dummy;

		(*repo)->values(object_names, false, values, dummy);
	}

	for (std::set<std::string>::const_iterator
		     b(object_names.begin()), e(object_names.end()); b != e;
	     ++b)
	{
		tobjrepoObj::values_t::const_iterator p=values.find(*b);

		if (p != values.end())
		{
			std::map<std::string, x::uuid>::const_iterator
				q(msg.uuids->objuuids.find(*b));

			if (q != msg.uuids->objuuids.end() &&
			    q->second == p->second.first)
				continue;
		}
		else
		{
			if (msg.uuids->objnouuids.find(*b) !=
			    msg.uuids->objnouuids.end())
				continue;
		}

		objserializer ack(*repo, *b, lock);
		getdst()->event(ack);
	}
}

void objrepocopysrcthreadObj::dispatch_event_slavelistready(const objrepocopy::slavelistready &msg)
{
	ENTERED(dispatch_event_slavelistready);

	LOG_DEBUG("Received SLAVELISTREADY");

	objrepocopy::slaveliststart ack;

	getdst()->event(ack);
}

void objrepocopysrcthreadObj::dispatch_event_slavelistdone(const objrepocopy::slavelistdone &msg)

{
	ENTERED(dispatch_event_slavelistdone);

	LOG_DEBUG("Received SLAVELISTDONE");

	(*complete_ptr)->setSuccesfull(*batonp);
	LOG_DEBUG("Invoking stop()");
	stop();
}

tobjrepoObj::commitlock_t objrepocopysrcthreadObj::getcommitlock()
{
	auto msgqueue=get_msgqueue();

	LOG_DEBUG("Acquiring commit lock");

	x::eventfd eventfd(msgqueue->get_eventfd());

	tobjrepoObj::commitlock_t commitLock=(*repo)->commitlock(eventfd);

	while (!commitLock->locked())
	{
		while (!msgqueue->empty())
			msgqueue->pop()->dispatch();

		eventfd->event();
	}
	LOG_DEBUG("Commit lock acquired");

	return commitLock;
}

void objrepocopysrcthreadObj::run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
				  start_threadmsgdispatcher_sync &sync_arg,
				  tobjrepo &repocopy,
				  batonptr &batonref,
				  copycomplete &ret,
				  const x::ptr<x::obj> &mcguffinref)
{
#ifdef DEBUG_THIS
	debug_this=DEBUG_COPYSRCTHREAD();
#endif
	msgqueue_auto msgqueue(this);
	threadmsgdispatcher_mcguffin=nullptr;
	sync_arg->thread_started();

#ifdef DEBUG_THIS
	debug_this.qptr=&*msgqueue;
#endif

	repo= &repocopy;
	dst=ret->dst;
	batonp= &batonref;

	try {
		{
			tobjrepoObj::commitlock_t commitLock=getcommitlock();

			auto notifier(x::ref<notifierObj>
				      ::create(dst, *repo));

			(*repo)->installNotifier(notifier);
			ret->setNotifier(notifier);
		}

		complete_ptr=&ret;

		uuidenum=x::ptr<uuidenumObj>();

		// Send a baton request

		getdst()->event(objrepocopy::batonrequest());

		while (1)
		{
			ENTERED(event_loop);
			msgqueue.event();
		}
	} catch (const x::stopexception &e)
	{
		LOG_DEBUG("Thread stopped");
	} catch (const x::exception &e)
	{
		LOG_FATAL(e);
	}
}

/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "objrepocopydstthread.H"
#include "objrepocopysrcinterface.H"
#include "objuuidenumerator.H"
#include "baton.H"
#include "tobjrepo.H"

LOG_CLASS_INIT(objrepocopydstthreadObj);

class objrepocopydstthreadObj::dispatch_lock : public std::lock_guard<std::mutex> {

	const char *function;
public:
	dispatch_lock(std::mutex &mutex,
		      const char *functionArg);
	~dispatch_lock();
};

objrepocopydstthreadObj::dispatch_lock::dispatch_lock(std::mutex &mutex,
						      const char *functionArg)
	: std::lock_guard<std::mutex>(mutex), function(functionArg)
{
	LOG_DEBUG("Begin dispatch " << function);
}

objrepocopydstthreadObj::dispatch_lock::~dispatch_lock()
{
	LOG_DEBUG("End dispatch " << function);
}

#define DISPATCH(name) dispatch_lock lock(dispatch_mutex, # name)

class objrepocopydstthreadObj::uuidenumObj : public objuuidenumeratorObj {

	objrepocopydstthreadObj *parent;

public:
	uuidenumObj(const tobjrepo &repoArg,
		    objrepocopydstthreadObj *parentArg);
	~uuidenumObj();

	objuuidlist nextbatch(const std::set<std::string> &objectnames)
		override;
};

objrepocopydstthreadObj::uuidenumObj::uuidenumObj(const tobjrepo &repoArg,
						  objrepocopydstthreadObj
						  *parentArg)
 : objuuidenumeratorObj(repoArg),
			      parent(parentArg)
{
}

objrepocopydstthreadObj::uuidenumObj::~uuidenumObj()
{
}

objuuidlist
objrepocopydstthreadObj::uuidenumObj::nextbatch(const std::set<std::string>
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

objrepocopydstthreadObj::objrepocopydstthreadObj(const std::string &nameArg)
	: name(nameArg)
{
}

objrepocopydstthreadObj::~objrepocopydstthreadObj()
{
}

std::string objrepocopydstthreadObj::getName() const noexcept
{
	return name;
}

objrepocopysrcinterface objrepocopydstthreadObj::getsrc()
{
	objrepocopysrcinterfaceptr srcref(src.getptr());

	if (srcref.null())
		throw EXCEPTION("Lost connection to master");

	return srcref;
}

void objrepocopydstthreadObj::event(const objrepocopy::batonrequest &msg)

{
	event_batonrequest();
}

void objrepocopydstthreadObj::event(const objrepocopy::masterlist &msg)

{
	event_masterlist(msg);
}

void objrepocopydstthreadObj::event(const objrepocopy::masterlistdone &msg)

{
	event_masterlistdone(msg);
}

void objrepocopydstthreadObj::event(const objrepocopy::slaveliststart &msg)

{
	event_slaveliststart(msg);
}

void objrepocopydstthreadObj::event(const objrepocopy::masterack &msg)

{
	event_masterack(msg);
}

void objrepocopydstthreadObj::dispatch_event_batonrequest()
{
	LOG_DEBUG("Received BATONREQUEST");

	objrepocopy::batonresponse ack;

	if (!batonp->null())
		ack.uuid=x::to_string((*batonp)->batonuuid);

	getsrc()->event(ack);
}

void objrepocopydstthreadObj::dispatch_event_masterlist(const objrepocopy::masterlist &msg)

{
	*batonp=batonptr();

	objrepocopy::slavelist ack;

	process_dispatch(msg, ack);

	getsrc()->event(ack);
}

void objrepocopydstthreadObj
::process_dispatch(const objrepocopy::masterlist &msg,
		   objrepocopy::slavelist &ack)

{
	DISPATCH(masterlist);

	tobjrepoObj::values_t values;

	ack.uuids=objuuidlist::create();

	auto msgqueue=get_msgqueue();

	{
		std::set<std::string> names;

		for (std::map<std::string, x::uuid>::const_iterator
			     b(msg.uuids->objuuids.begin()),
			     e(msg.uuids->objuuids.end()); b != e; ++b)
		{
			names.insert(b->first);
		}

		x::eventfd eventfd(msgqueue->get_eventfd());

		tobjrepoObj::lockentry_t lock((*repo)->lock(names, eventfd));

		while (!lock->locked())
		{

			while (!msgqueue->empty())
				msgqueue->pop()->dispatch();

#ifdef DST_DEBUG_WAIT_LOCK
			DST_DEBUG_WAIT_LOCK();
#endif
			eventfd->event();
		}

#ifdef DST_DEBUG_AFTER_LOCK
		DST_DEBUG_AFTER_LOCK();
#endif

		(*repo)->values(names, false, values, ack.uuids->objnouuids);
	}

	for (tobjrepoObj::values_t::const_iterator
		     b(values.begin()), e(values.end()); b != e; ++b)
	{
		if (msg.uuids->objuuids.find(b->first)->second
		    == b->second.first)
			continue;

		ack.uuids->objuuids.insert(std::make_pair(b->first,
							  b->second.first));
	}
}

void objrepocopydstthreadObj::dispatch_event_masterlistdone(const objrepocopy::masterlistdone &msg)
{
	LOG_DEBUG("Received MASTERLISTDONE");
	objrepocopy::slavelistready ack;

	getsrc()->event(ack);
}

void objrepocopydstthreadObj::dispatch_event_slaveliststart(const objrepocopy::slaveliststart &msg)
{
	LOG_DEBUG("Received SLAVELISTSTART");

	uuidenumref=x::ptr<uuidenumObj>::create(*repo, this);

	objrepocopy::masterack dummy;

	dispatch_event_masterack(dummy);
}

void objrepocopydstthreadObj::dispatch_event_masterack(const objrepocopy::masterack &msg)

{
	objuuidlist uuids;

	{
		DISPATCH(masterack);

		uuids=uuidenumref->next();
	}

	if (uuids.null())
	{
		objrepocopy::slavelistdone ack;

		getsrc()->event(ack);
	}
	else
	{
		objrepocopy::slavelist ack;

		ack.uuids=uuids;
		getsrc()->event(ack);
	}
}

void objrepocopydstthreadObj::event(const objrepocopy::copycomplete &msg)

{
	flag->flag=true;
	stop();
}

void objrepocopydstthreadObj::event(const objserializer &msg)

{
	throw EXCEPTION("Internal error: objserializer message received where it shouldn't be");
}

void objrepocopydstthreadObj::run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
				  start_threadmsgdispatcher_sync &sync_arg,
				  tobjrepo &repocpy,
				  x::weakptr<objrepocopysrcinterfaceptr> &srcArg,
				  boolref &flagref,
				  batonptr &batonref,
				  const x::ptr<x::obj> &mcguffin)
{
	msgqueue_auto msgqueue(this);
	threadmsgdispatcher_mcguffin=nullptr;
	sync_arg->thread_started();
	repo= &repocpy;
	src=srcArg;
	flag= &*flagref;
	batonp= &batonref;

	try {
		while (1)
			msgqueue.event();
	} catch (const x::stopexception &e)
	{
	} catch (const x::exception &e)
	{
		LOG_FATAL(e);
	}
}

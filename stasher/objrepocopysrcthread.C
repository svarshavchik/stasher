/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "objrepocopysrcthread.H"
#include "objrepocopydstinterface.H"
#include "objuuidenumerator.H"
#include "objserializer.H"
#include "tobjrepo.H"
#include "baton.H"

#include <x/threads/run.H>

LOG_CLASS_INIT(objrepocopysrcthreadObj);

class objrepocopysrcthreadObj::dispatch_lock : public std::lock_guard<std::mutex> {

	const char *function;
public:
	dispatch_lock(std::mutex &mutex,
		      const char *functionArg);
	~dispatch_lock() noexcept;
};

objrepocopysrcthreadObj::dispatch_lock::dispatch_lock(std::mutex &mutex,
						      const char *functionArg)
	: std::lock_guard<std::mutex>(mutex), function(functionArg)
{
	LOG_DEBUG("Begin dispatch " << function);
}

objrepocopysrcthreadObj::dispatch_lock::~dispatch_lock() noexcept
{
	LOG_DEBUG("End dispatch " << function);
}

#define DISPATCH(name) dispatch_lock lock(dispatch_mutex, # name)

class objrepocopysrcthreadObj::uuidenumObj : public objuuidenumeratorObj {

	objrepocopysrcthreadObj *parent;

public:
	uuidenumObj(const tobjrepo &repoArg,
		    objrepocopysrcthreadObj *parentArg);
	~uuidenumObj() noexcept;

	objuuidlist nextbatch(const std::set<std::string> &objectnames)
;
};

objrepocopysrcthreadObj::uuidenumObj::uuidenumObj(const tobjrepo &repoArg,
						  objrepocopysrcthreadObj
						  *parentArg)
 : objuuidenumeratorObj(repoArg),
			      parent(parentArg)
{
}

objrepocopysrcthreadObj::uuidenumObj::~uuidenumObj() noexcept
{
}

objuuidlist
objrepocopysrcthreadObj::uuidenumObj::nextbatch(const std::set<std::string>
						&objectnames)

{
	x::eventfd eventfd(parent->msgqueue->getEventfd());

	tobjrepoObj::lockentry_t lock(repo->lock(objectnames, eventfd));

	while (!lock->locked())
	{
		while (!parent->msgqueue->empty())
			parent->msgqueue->pop()->dispatch();

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
	~notifierObj() noexcept;

	void installed(const std::string &objname,
		       const x::ptr<x::obj> &lock)
;

	void removed(const std::string &objname,
		       const x::ptr<x::obj> &lock)
;
};

objrepocopysrcthreadObj::notifierObj
::notifierObj(const x::weakptr<objrepocopydstinterfaceptr > &dstArg,
	      const tobjrepo &repoArg)
	: dst(dstArg), repo(repoArg)
{
}

objrepocopysrcthreadObj::notifierObj::~notifierObj() noexcept
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

objrepocopysrcthreadObj::copycompleteObj::~copycompleteObj() noexcept
{
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

objrepocopysrcthreadObj::~objrepocopysrcthreadObj() noexcept
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

void objrepocopysrcthreadObj::dispatch(const event_batonresponse_msg &msg)

{
	if (!batonp->null() && x::tostring((*batonp)->batonuuid) == msg.msg.uuid)
	{
		// Short-circuited

#ifdef DEBUG_BATON_TEST_6_VERIFY
		DEBUG_BATON_TEST_6_VERIFY();
#endif

		dispatch(event_slavelistdone_msg(objrepocopy::slavelistdone()));
		return;
	}

	*batonp=batonptr();

	uuidenum=x::ptr<uuidenumObj>::create(*repo, this);

	// Simulate a dummy slavelist response, in order to kick-start the
	// masterlist messages
	objrepocopy::slavelist dummy;

	dummy.uuids=objuuidlist::create();
	dispatch(dummy);
}

void objrepocopysrcthreadObj::dispatch(const event_slavelist_msg &msg)

{
	void (objrepocopysrcthreadObj::*ack_func)(objuuidlist &uuidlist)
;

	objuuidlist uuid;

	dispatch_handle_slavelist(msg.msg);

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
	objrepocopy::masterlistdone ack;

	getdst()->event(ack);
}

void objrepocopysrcthreadObj
::dispatch_ack_slavelist_masterlist(objuuidlist &uuidList)

{
	objrepocopy::masterlist ack;

	ack.uuids=uuidList;
	getdst()->event(ack);
}

void objrepocopysrcthreadObj
::dispatch_ack_slavelist_masterack(objuuidlist &uuidList)

{
	objrepocopy::masterack ack;
	getdst()->event(ack);
}

void objrepocopysrcthreadObj
::dispatch_handle_slavelist(const objrepocopy::slavelist &msg)

{
	std::set<std::string> object_names=msg.uuids->objnouuids;

	tobjrepoObj::values_t values;

	for (std::map<std::string, x::uuid>::const_iterator
		     b(msg.uuids->objuuids.begin()),
		     e(msg.uuids->objuuids.end()); b != e; ++b)
		object_names.insert(b->first);

	tobjrepoObj::lockentryptr_t lock;

	if (!object_names.empty())
	{
		x::eventfd eventfd(msgqueue->getEventfd());

		lock=(*repo)->lock(object_names, eventfd);

		while (!lock->locked())
		{
			while (!msgqueue->empty())
				msgqueue->pop()->dispatch();

			eventfd->event();
		}

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

void objrepocopysrcthreadObj::dispatch(const event_slavelistready_msg &msg)

{
	objrepocopy::slaveliststart ack;

	getdst()->event(ack);
}

void objrepocopysrcthreadObj::dispatch(const event_slavelistdone_msg &msg)

{
	(*complete_ptr)->setSuccesfull(*batonp);
	stop();
}

tobjrepoObj::commitlock_t objrepocopysrcthreadObj::getcommitlock()

{
	x::eventfd eventfd(msgqueue->getEventfd());

	tobjrepoObj::commitlock_t commitLock=(*repo)->commitlock(eventfd);

	while (!commitLock->locked())
	{
		while (!msgqueue->empty())
			msgqueue->pop()->dispatch();

		eventfd->event();
	}

	return commitLock;
}

void objrepocopysrcthreadObj::run(tobjrepo &repocopy,
				  batonptr &batonref,
				  copycomplete &ret,
				  const x::ptr<x::obj> &mcguffinref)
{
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
			msgqueue->pop()->dispatch();
	} catch (const x::stopexception &e)
	{
	} catch (const x::exception &e)
	{
		LOG_FATAL(e);
	}
}

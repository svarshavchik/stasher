/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "fdobjreaderthread.H"
#include <x/sysexception.H>

STASHER_NAMESPACE_START

// Capture reads from the input iterator

class LIBCXX_HIDDEN fdobjreaderthreadObj::fdadapterObj
	: public x::fdbaseObj::adapterObj {

	// My object.
	fdobjreaderthreadObj &me;
public:
	fdadapterObj(const x::fdbase &baseArg,
		     fdobjreaderthreadObj &meArg);

	~fdadapterObj() noexcept;

	//! Intercept reads.

	size_t pubread(char *buffer, size_t cnt);
};

fdobjreaderthreadObj::fdadapterObj::fdadapterObj(const x::fdbase &baseArg,
						 fdobjreaderthreadObj &meArg)
	: x::fdbaseObj::adapterObj(baseArg), me(meArg)
{
}

fdobjreaderthreadObj::fdadapterObj::~fdadapterObj() noexcept
{
}

size_t fdobjreaderthreadObj::fdadapterObj::pubread(char *buffer, size_t cnt)

{
	size_t n=pubread_pending();

	if (n > 0)
	{
		if (n > cnt)
			n=cnt;

		return ptr->pubread(buffer, cnt);
	}

	return me.pubread(ptr, buffer, cnt);
}

fdobjreaderthreadObj::fdobjreaderthreadObj()
{
}

fdobjreaderthreadObj::~fdobjreaderthreadObj() noexcept
{
}

void fdobjreaderthreadObj::drain()
{
	msgqueue_t msgqueue=get_msgqueue();

	try {
		msgqueue->getEventfd()->event();
		//! Consume anything
	} catch (const x::sysexception &e)
	{
		int n=e.getErrorCode();

		if (n != EAGAIN && n != EWOULDBLOCK)
			throw;
	}

	while (!msgqueue->empty())
		msgqueue->pop()->dispatch();
}

void fdobjreaderthreadObj::mainloop(msgqueue_auto &msgqueue,
				    const x::fdbase &transport,
				    x::fd::base::inputiter &beg_iter)

{
	msgqueue->getEventfd()->nonblock(true);
	x::fd::base::inputiter b(beg_iter), e;

	beg_iter=x::fd::base::inputiter();

	auto adapter=x::ptr<fdadapterObj>::create(b.buffer(), *this);

	b.update(adapter);

	x::fdptr socketref;

	socket=&socketref;

	{
		x::fdObj *p=dynamic_cast<x::fdObj *>(&*transport);

		// In some cases this is something else

		if (p)
			socketref=x::fd(p);
	}

	mainloop_impl(b, e);
}

STASHER_NAMESPACE_END

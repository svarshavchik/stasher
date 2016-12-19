/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "objrepocopysrc.H"
#include "baton.H"
#include <x/threadmsgdispatcher.H>

#include "threadmgr.H"

objrepocopysrcObj::objrepocopysrcObj()
{
}

objrepocopysrcObj::~objrepocopysrcObj() noexcept
{
	stop();
	wait();
}

objrepocopysrcObj::copycomplete
objrepocopysrcObj::start(const tobjrepo &repoArg,
			 const objrepocopydstinterfaceptr &srcArg,
			 const batonptr &batonArg,
			 const x::ptr<x::obj> &mcguffin)

{
	objrepocopysrcObj::copycomplete cc=
		objrepocopysrcObj::copycomplete::create(srcArg);

	auto thr=x::ref<objrepocopysrcthreadObj>::create("objrepocopydst");

	this->start_thread(thr, repoArg, batonArg, cc, mcguffin);

	return cc;
}

void objrepocopysrcObj::event(const objrepocopy::batonresponse &msg)

{
	thread_owner_t::event(msg);
}

void objrepocopysrcObj::event(const objrepocopy::slavelist &msg)

{
	thread_owner_t::event(msg);
}

void objrepocopysrcObj::event(const objrepocopy::slavelistready &msg)

{
	thread_owner_t::event(msg);
}

void objrepocopysrcObj::event(const objrepocopy::slavelistdone &msg)

{
	thread_owner_t::event(msg);
}

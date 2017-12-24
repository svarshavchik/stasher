/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "objrepocopydst.H"
#include "objrepocopysrc.H"
#include "tobjrepo.H"
#include "baton.H"
#include <x/threadmsgdispatcher.H>

objrepocopydstObj::objrepocopydstObj()
{
}

objrepocopydstObj::~objrepocopydstObj()
{
	stop();
	wait();
}

void objrepocopydstObj::start(const tobjrepo &repoArg,
			      const objrepocopysrcinterfaceptr &srcArg,
			      const boolref &flagArg,
			      const batonptr &batonArg,
			      const x::ptr<x::obj> &mcguffinArg)

{
	auto thr=x::ref<objrepocopydstthreadObj>::create("objrepocopydst");

	this->start_threadmsgdispatcher(thr, repoArg,
			   x::weakptr<objrepocopysrcinterfaceptr>(srcArg),
			   flagArg, batonArg, mcguffinArg);
}

void objrepocopydstObj::event(const objrepocopy::batonrequest &msg)

{
	thread_owner_t::event(msg);
}

void objrepocopydstObj::event(const objrepocopy::masterlist &msg)

{
	thread_owner_t::event(msg);
}

void objrepocopydstObj::event(const objrepocopy::masterlistdone &msg)

{
	thread_owner_t::event(msg);
}

void objrepocopydstObj::event(const objrepocopy::slaveliststart &msg)

{
	thread_owner_t::event(msg);
}

void objrepocopydstObj::event(const objrepocopy::masterack &msg)

{
	thread_owner_t::event(msg);
}

void objrepocopydstObj::event(const objrepocopy::copycomplete &msg)

{
	thread_owner_t::event(msg);
}

void objrepocopydstObj::event(const objserializer &msg)

{
	thread_owner_t::event(msg);
}

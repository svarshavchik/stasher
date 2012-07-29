/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objrepocopydstthreadfwd_H
#define objrepocopydstthreadfwd_H

#include <x/ptr.H>
#include <x/ref.H>

class objrepocopydstthreadObj;

//! A reference to a objrepocopydstthreadObj

typedef x::ref<objrepocopydstthreadObj> objrepocopydstthread;

//! A nullable reference to a objrepocopydstthreadObj

typedef x::ptr<objrepocopydstthreadObj> objrepocopydstthreadptr;

#endif

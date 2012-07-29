/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objrepocopydstfwd_H
#define objrepocopydstfwd_H

#include <x/ptr.H>
#include <x/ref.H>

class objrepocopydstObj;

//! A reference to a objrepocopydstObj

typedef x::ref<objrepocopydstObj> objrepocopydst;

//! A nullable reference to a objrepocopydstObj

typedef x::ptr<objrepocopydstObj> objrepocopydstptr;

#endif

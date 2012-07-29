/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objrepocopysrcfwd_H
#define objrepocopysrcfwd_H

#include <x/ref.H>
#include <x/ptr.H>

class objrepocopysrcObj;

//! A reference to a objrepocopysrcObj

typedef x::ref<objrepocopysrcObj> objrepocopysrc;

//! A nullable pointer reference to a objrepocopysrcObj

typedef x::ptr<objrepocopysrcObj> objrepocopysrcptr;

#endif

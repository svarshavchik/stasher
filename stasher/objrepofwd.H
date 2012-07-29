/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objrepofwd_H
#define objrepofwd_H

#include <x/ref.H>
#include <x/ptr.H>

class objrepoObj;

//! A reference to an object repository class instance.

typedef x::ref<objrepoObj> objrepo;

//! A nullable pointer reference to an object repository class instance.

typedef x::ptr<objrepoObj> objrepoptr;

#endif

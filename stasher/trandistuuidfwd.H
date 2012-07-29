/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef trandistuuidfwd_H
#define trandistuuidfwd_H

#include <x/ref.H>
#include <x/ptr.H>

class trandistuuidObj;

//! A reference to a trandistuuidObj object

typedef x::ref<trandistuuidObj> trandistuuid;

//! A nullable reference pointer to a trandistuuidObj object

typedef x::ptr<trandistuuidObj> trandistuuidptr;

#endif

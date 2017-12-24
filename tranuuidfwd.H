/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef tranuuidfwd_H
#define tranuuidfwd_H

#include <x/ref.H>
#include <x/ptr.H>

class tranuuidObj;

//! A reference to a tranuuidObj object

typedef x::ref<tranuuidObj> tranuuid;

//! A nullable reference pointer to a tranuuidObj object

typedef x::ref<tranuuidObj> tranuuidptr;

#endif

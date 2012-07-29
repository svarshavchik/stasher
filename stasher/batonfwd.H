/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef batonfwd_H
#define batonfwd_H

#include "objrepo_config.h"

#include <x/ptr.H>
#include <x/ref.H>

class batonObj;

//! A reference to a batonObj.

typedef x::ref<batonObj> baton;

//! A nullable reference pointer to a batonObj.

typedef x::ptr<batonObj> batonptr;

#endif

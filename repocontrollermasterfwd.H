/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef repocontrollermasterfwd_H
#define repocontrollermasterfwd_H

#include <x/ptr.H>
#include <x/ref.H>

class repocontrollermasterObj;

//! A reference to a repocontrollermasterObj

typedef x::ref<repocontrollermasterObj> repocontrollermaster;

//! A nullable reference to a repocontrollermasterObj

typedef x::ptr<repocontrollermasterObj> repocontrollermasterptr;

#endif

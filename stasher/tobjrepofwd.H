/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef tobjrepofwd_H
#define tobjrepofwd_H

#include <x/ref.H>
#include <x/ptr.H>

class tobjrepoObj;

//! A reference to a transactional object repository.
typedef x::ref<tobjrepoObj> tobjrepo;

//! A nullable reference to a transactional object repository.
typedef x::ptr<tobjrepoObj> tobjrepoptr;

#endif

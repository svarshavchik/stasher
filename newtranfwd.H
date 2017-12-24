/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef newtranfwd_H
#define newtranfwd_H

#include <x/ptr.H>
#include <x/ref.H>

class newtranObj;

//! A reference to the new transaction
typedef x::ref<newtranObj> newtran;

//! A nullable reference pointer to the new transaction
typedef x::ptr<newtranObj> newtranptr;

#endif

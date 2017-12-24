/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef boolreffwd_H
#define boolreffwd_H

#include <x/ptr.H>
#include <x/ref.H>

class boolrefObj;

//! A reference to a boolrefObj

typedef x::ref<boolrefObj> boolref;

//! A nullable reference to a boolrefObj

typedef x::ptr<boolrefObj> boolrefptr;

#endif

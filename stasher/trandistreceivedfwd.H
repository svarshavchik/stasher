/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef trandistreceivedfwd_H
#define trandistreceivedfwd_H

#include <x/ref.H>
#include <x/ptr.H>

class trandistreceivedObj;

//! A reference to a trandistreceivedObj object

typedef x::ref<trandistreceivedObj> trandistreceived;

//! A nullable pointer reference to a trandistreceivedObj object

typedef x::ptr<trandistreceivedObj> trandistreceivedptr;

#endif

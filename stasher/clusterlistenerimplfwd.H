/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef clusterlistenerimplfwd_H
#define clusterlistenerimplfwd_H

#include <x/ptr.H>
#include <x/ref.H>

class clusterlistenerimplObj;

//! A reference to a clusterlistenerimplObj

typedef x::ref<clusterlistenerimplObj> clusterlistenerimpl;

//! A nullable reference to a clusterlistenerimplObj

typedef x::ptr<clusterlistenerimplObj> clusterlistenerimplptr;

#endif

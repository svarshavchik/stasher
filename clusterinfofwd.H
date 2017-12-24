/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef clusterinfofwd_H
#define clusterinfofwd_H

#include <x/ref.H>
#include <x/ptr.H>

class clusterinfoObj;

//! A reference to a clusterinfoObj

typedef x::ref<clusterinfoObj> clusterinfo;

//! A nullable pointer reference to a clusterinfoObj

typedef x::ptr<clusterinfoObj> clusterinfoptr;

#endif



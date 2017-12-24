/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef clusterlistenerfwd_H
#define clusterlistenerfwd_H

#include <x/ptr.H>
#include <x/ref.H>

class clusterlistenerObj;

//! A reference to a clusterlistenerObj

typedef x::ref<clusterlistenerObj> clusterlistener;

//! A nullable reference to a clusterlistenerObj

typedef x::ptr<clusterlistenerObj> clusterlistenerptr;

#endif

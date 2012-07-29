/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stoppablethreadtrackerfwd_H
#define stoppablethreadtrackerfwd_H

#include <x/ref.H>
#include <x/ptr.H>

#include "stasher/namespace.H"

STASHER_NAMESPACE_START

class stoppableThreadTrackerObj;
class stoppableThreadTrackerImplObj;

//! A reference to a stoppable thread tracker object.

typedef x::ref<stoppableThreadTrackerImplObj> stoppableThreadTrackerImpl;

//! A nullable reference pointer to a stoppable thread tracker object.

typedef x::ptr<stoppableThreadTrackerImplObj> stoppableThreadTrackerImplptr;

//! A reference to a stoppable thread tracker object

typedef x::ref<stoppableThreadTrackerObj> stoppableThreadTracker;

//! A nullable reference pointer to a stoppable thread tracker object

typedef x::ptr<stoppableThreadTrackerObj> stoppableThreadTrackerptr;

STASHER_NAMESPACE_END


#endif

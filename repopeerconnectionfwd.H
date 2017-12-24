/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef repopeerconnectionfwd_H
#define repopeerconnectionfwd_H

#include <x/ptr.H>
#include <x/ref.H>

class repopeerconnectionObj;

//! A reference to a repopeerconnectionObj

typedef x::ref<repopeerconnectionObj> repopeerconnection;

//! A nullable reference to a repopeerconnectionObj

typedef x::ptr<repopeerconnectionObj> repopeerconnectionptr;

#endif

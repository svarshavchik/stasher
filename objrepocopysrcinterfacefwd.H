/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objrepocopysrcinterfacefwd_H
#define objrepocopysrcinterfacefwd_H

#include <x/ptr.H>
#include <x/ref.H>

class objrepocopysrcinterfaceObj;

//! A reference to a objrepocopysrcinterfaceObj

typedef x::ref<objrepocopysrcinterfaceObj> objrepocopysrcinterface;

//! A nullable reference to a objrepocopysrcinterfaceObj

typedef x::ptr<objrepocopysrcinterfaceObj> objrepocopysrcinterfaceptr;

#endif

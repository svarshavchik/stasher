/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objrepocopydstinterfacefwd_H
#define objrepocopydstinterfacefwd_H

#include <x/ptr.H>
#include <x/ref.H>

class objrepocopydstinterfaceObj;

//! A reference to a objrepocopydstinterfaceObj

typedef x::ref<objrepocopydstinterfaceObj> objrepocopydstinterface;

//! A nullable reference to a objrepocopydstinterfaceObj

typedef x::ptr<objrepocopydstinterfaceObj> objrepocopydstinterfaceptr;

#endif

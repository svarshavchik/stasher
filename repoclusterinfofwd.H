/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef repoclusterinfofwd_H
#define repoclusterinfofwd_H

#include <x/ref.H>
#include <x/ptr.H>

class repoclusterinfoObj;

//! A reference to a repoclusterinfoObj

typedef x::ref<repoclusterinfoObj> repoclusterinfo;

//! A nullable reference pointer to a repoclusterinfoObj

typedef x::ptr<repoclusterinfoObj> repoclusterinfoptr;

#endif

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef tranfwd_H
#define tranfwd_H

class tranObj;

//! A reference to a transaction object
typedef x::ref<tranObj> tran;

//! A nullable pointer reference to a transaction object
typedef x::ptr<tranObj> tranptr;

#endif

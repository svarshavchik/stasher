/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef trancommitfwd_H
#define trancommitfwd_H

class trancommitObj;

//! A reference to a transaction object
typedef x::ref<trancommitObj> trancommit;

//! A nullable reference to a transaction object
typedef x::ptr<trancommitObj> trancommitptr;

#endif

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef boolref_H
#define boolref_H

#include <x/obj.H>
#include "boolreffwd.H"

//! A reference-counted object that contains a bool member called "flag"

class boolrefObj : virtual public x::obj {

public:
	//! The constructor initializes the flag to \c false
	boolrefObj();

	//! Destructor
	~boolrefObj() noexcept;

	//! Status
	bool flag;
};

#endif

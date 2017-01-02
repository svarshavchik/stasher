/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef tran_H
#define tran_H

#include "tranmeta.H"
#include "tranfwd.H"

//! An existing transaction

//! Not instantiated directly. Instantiate by some method of tobjrepoObj.
//!
class tranObj : public tranmeta, virtual public x::obj {

public:
	//! Transaction identifier
	x::uuid uuid;

	//! Default constructor
	tranObj();

	//! Constructor
	tranObj(const x::uuid &uuidArg);
	~tranObj();
};

#endif

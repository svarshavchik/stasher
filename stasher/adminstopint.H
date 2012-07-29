/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef adminstopint_H
#define adminstopint_H

#include <x/ref.H>
#include <x/ptr.H>
#include <x/obj.H>
#include <x/fd.H>
#include "stasher/namespace.H"

//! Command: server shutdown

class adminstopintObj : virtual public x::obj {

public:
	adminstopintObj();
	~adminstopintObj() noexcept;

	//! An admin connection thread is requesting to stop this instance

	virtual void stop(//! Admin connection socket. 

			  //! It's stashed away, so it only actually gets
			  //! closed after everything stops.

			  const x::fd &socket)=0;
};

//! A reference to adminstopintObj

typedef x::ref<adminstopintObj> adminstopint;

//! A nullable reference pointer to adminstopintObj

typedef x::ptr<adminstopintObj> adminstopintptr;

#endif

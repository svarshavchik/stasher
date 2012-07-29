/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_versionedptr_H
#define stasher_versionedptr_H

#include <stasher/namespace.H>
#include <x/ptr.H>
#include <x/ref.H>
#include <x/obj.H>

STASHER_NAMESPACE_START

template<typename ptr_type> class versionedptr;

//! An x::ptr, versioned by a mcguffin.

//! This template itself is undefined. Only a specialization is defined for
//! an x::ptr, so the template parameter must be an x::ptr.
//! This template defines
//! a subclass of the x::ptr which maintains a mcguffin that's tied
//! to the current x::ptr. It only implements a default constructor, that
//! constructs a null x::ptr.
//!
//! * and -> operators, and null() are exposed from the superclass, so that
//! this subclass can be generally used like an x::ptr.
//!
//! This subclass associated the value of the x::ptr superclass with a
//! mcguffin that's paired with it.
//!
//! version() returns the current mcguffin value.
//!
//! There is an assignment operator that's defined that takes the x::ptr,
//! which sets the new value of
//! x::ptr subclass, and installs a new mcguffin, releasing the reference on the
//! mcguffin that's tied to the former value held by x::ptr.
//!
//! Pass this template as a second parameter to
//! currentObj, and use the mcguffin used as a notification mechanism when the
//! currentObj-held value changes.
//!
//! Custom-tailed copy/move constructor and the assignment operator do not
//! copy/move the mcguffin. Each versioned nullable pointer is considered to
//! be a separate entity, each with its own mcguffin. The custom constructor
//! and assignment operator set up a new mcguffin, during the copy/move.
//!
//! \note
//! No locking is implemented here. Thread safety is your responsibility.
//! Passing this as a second template parameter to currentObj protects this
//! object by a mutex.
//!
//! \note
//! For the held mcguffin to hold true to its purpose, do not retain the
//! strong reference returned by version().

#ifdef DOXYGEN

// Doxygen has problems with links to specializations
template<typename ptr_type>
class versionedptr
#else
template<typename objClass, typename baseClass>
class versionedptr<x::ptr<objClass, baseClass> >
#endif

{
	//! Convenience typedef
	typedef x::ptr<objClass, baseClass> ptr_type;

	//! The x::ptr that this is all about.
	ptr_type ptr;

	//! Convenience typedef
	typedef versionedptr<ptr_type> versioned_type;

	//! The mcguffin associated with the superclass's current value

	x::ref<x::obj> mcguffin;

public:
	//! Constructor
	versionedptr() : mcguffin(x::ref<x::obj>::create())
	{
	}

	//! Copy constructor sets up a new mcguffin
	versionedptr(const versioned_type &o)
		: ptr(o), mcguffin(x::ref<x::obj>::create())
	{
	}

	//! Move constructor sets up a new mcguffin
	versionedptr(versioned_type &&o)
		: ptr(std::move(o)), mcguffin(x::ref<x::obj>::create())
	{
	}

	//! Assignment operator sets up a new mcguffin

	versionedptr &operator=(const versioned_type &o)
	{
		auto old_mcguffin=mcguffin;

		ptr=o;
		mcguffin=x::ref<x::obj>::create();
		return *this;
	}

	//! Move operator sets up a new mcguffin

	versionedptr &operator=(versioned_type && o)
	{
		auto old_mcguffin=mcguffin;

		ptr=std::move(o);
		mcguffin=x::ref<x::obj>::create();
		return *this;
	}

	//! Pretend that we're the x::ptr, and assign another one to us.

	versioned_type &operator=(const ptr_type &ptrArg)
	{
		auto old_mcguffin=mcguffin;

		ptr=ptrArg;
		mcguffin=x::ref<x::obj>::create();
		return *this;
	}

	//! Pretend that we're the x::ptr, and assign another one to us.

	versioned_type &operator=(ptr_type && ptrArg)
	{
		auto old_mcguffin=mcguffin;

		ptr=std::move(ptrArg);
		mcguffin=x::ref<x::obj>::create();
		return *this;
	}

	//! Make it easier to assign a \c ptr_type from this object.
	operator const ptr_type &() const
	{
		return ptr;
	}

	//! Return the current mcguffin

	x::ref<x::obj> getversion() const
	{
		return mcguffin;
	}

	//! Forward the null() operator

	bool null() const
	{
		return ptr.null();
	}

	//! Forward the * operator

	objClass &operator*() const
	{
		return ptr.operator*();
	}

	//! Forward the -> operator

	objClass *operator->() const
	{
		return ptr.operator->();
	}
};

STASHER_NAMESPACE_END

#endif

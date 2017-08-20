/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_current_H
#define stasher_current_H

#include <stasher/namespace.H>
#include <stasher/currentbase.H>
#include <stasher/versionedptr.H>
#include <x/mpobj.H>

STASHER_NAMESPACE_START

//! A sample implementation of a currentBaseObj subclass.

//! Defines a mutex-protected "current_value_t current_value" member that
//! contains the x::ptr template parameter, a boolean flag, and a
//! req_stat_t.
//!
//! Implements updated() that sets the x::ptr value, and the isinitial flag.
//! Implements connection_update() that sets the connection_status.
//!
//! The first template parameter is an x::ptr to an object that's described by
//! create_managed_object().
//!
//! An optional second template parameter defaults to the first, x::ptr
//! template parameter, and defines the type held inside a mutex-protected
//! object. If different, it must implement a move assignment operator,
//! that's invoked from update().
//!
//! make_versioned_current() uses \ref versionedptr "versionedptr<ptr_type>"
//! as the second template parameter.

template<typename ptr_type, typename obj_type=ptr_type>
class currentObj : public currentBaseObj<ptr_type> {

public:

	//! A container for the current object value, and the conncetion status.

	class value_t {
	public:
		//! The most recent object received by update().

		//! This is an x::ptr, which is null if the object was removed
		//! from the repository, or never existed there.
		obj_type value;

		//! Whether value is the initial object value.

		//! The first value received from the object repository, after
		//! instantiation, is considered an initial value. The first
		//! update() sets it to \c true, the subsequent updates set it
		//! to false. If the connection the object repository breaks,
		//! when it gets reestablished the now-current value of the
		//! object is update()d, and \c isinitial gets set to true
		//! again (this the value of the object might've changed),
		//! until the next update.
		bool isinitial;

		//! The current status of the managed object subscription.

		//! This gets set to \c req_disconnected_stat when the
		//! connection to the object repository breaks; and to
		//! \c req_processed_stat when the subscription updates have
		//! been established. Other values indicate an error at
		//! subscription time.
		req_stat_t connection_status;

		//! Constructor.
		value_t() : isinitial(false),
			    connection_status(req_disconnected_stat)
		{
		}
	};

	//! The metadata is stored in a mutex-protected object, with a condition variable.
	typedef x::mpcobj<value_t> current_value_t;

	//! The most recent value of the object and the connection status.
	current_value_t current_value;

	//! Constructor
	currentObj() {}

	//! Destructor.
	~currentObj() {}

	//! New value of the object received from the object repository.

	void update(const ptr_type &newvalue, bool isinitial) override
	{
		typename current_value_t::lock lock(current_value);

		lock->value=newvalue;
		lock->isinitial=isinitial;

		lock.notify_all();
	}

	//! Update the conncetion status to the object repository.
	void connection_update(req_stat_t status) override
	{
		typename current_value_t::lock lock(current_value);

		lock->connection_status=status;
		lock.notify_all();
	}
};

//! Provide a base class for pointers and references to currentObj

//! The base class provides convenient typedefs.
//! currentBaseObj is taken, so we must use this.

template<typename ptr_type, typename obj_type=ptr_type>
class currentptrrefBase : public x::ptrref_base {

public:

	//! Convenience typedef

	typedef typename currentObj<ptr_type, obj_type>::current_value_t
	current_value_t;

};

//! A reference to a basic currentBaseObj implementation.

//! \note
//! This is a template alias for an \c x::ref to a currentObj.
//!
//! \see currentObj

template<typename ptr_type, typename obj_type=ptr_type>

#ifdef DOXYGEN
class current {}; // TODO
#else
using current=x::ref<currentObj<ptr_type, obj_type>,
		     currentptrrefBase<ptr_type, obj_type> >;
#endif

//! A nullable pointer reference to a basic currentBaseObj implementation.

//! \note
//! This is a template alias for an \c x::ptr to a currentObj.
//!
//! \see currentObj

template<typename ptr_type, typename obj_type=ptr_type>
#ifdef DOXYGEN
class currentptr {}; // TODO
#else
using currentptr=x::ptr<currentObj<ptr_type, obj_type>,
			currentptrrefBase<ptr_type, obj_type> >;
#endif

STASHER_NAMESPACE_END

#endif

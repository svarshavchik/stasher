/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

/*! \file
**
** Definition of make_versioned_current().
*/

#ifndef stasher_versionedcurrent_H
#define stasher_versionedcurrent_H

#include <stasher/namespace.H>
#include <stasher/current.H>
#include <x/mpobj.H>

STASHER_NAMESPACE_START

//! Object created by make_versioned_current().

template<typename ptr_type, typename obj_type,
	 typename UpdateFunctor, typename ConnUpdateFunctor>
class versionedCurrentObj : public currentObj<ptr_type, obj_type> {

	//! The saved update functor
	typename std::enable_if<
		!std::is_lvalue_reference<UpdateFunctor>::value &&
	!std::is_rvalue_reference<UpdateFunctor>::value, UpdateFunctor>
		::type upd_funct;

	//! The saved conncetion update functor
	typename std::enable_if<
		!std::is_rvalue_reference<ConnUpdateFunctor>::value &&
	!std::is_lvalue_reference<ConnUpdateFunctor>::value, ConnUpdateFunctor>
		::type conn_upd_funct;

	//! Convenience typedef
	typedef currentObj<ptr_type, obj_type> superclass_t;

public:

	//! The constructor saves the functors

	template<typename UpdateFunctorArg,
		 typename ConnUpdateFunctorArg>
	versionedCurrentObj(UpdateFunctorArg && upd_functArg,
			    ConnUpdateFunctorArg && conn_upd_funcArg)
		: upd_funct(std::move(upd_functArg)),
		  conn_upd_funct(std::move(conn_upd_funcArg))
	{
	}

	//! Destructor
	~versionedCurrentObj()
	{
	}

	//! Override the superclass \%update() method.

	//! After calling \%update() in the superclass, invoke the update
	//! functor, forwarding to it the parameters.

	void update(const ptr_type &newvalue, bool isinitial) override
	{
		superclass_t::update(newvalue, isinitial);
		upd_funct(newvalue, isinitial);
	}

	//! Override the superclass connection_update() method.

	//! After calling connection_update() in the superclass, invoke the
	//! connection update functor, forwarding to it the updated connection
	//! status.

	void connection_update(req_stat_t status) override
	{
		superclass_t::connection_update(status);
		conn_upd_funct(status);
	}
};

//! Construct a currentObj that uses a versionedptr, with notification functors.

//! The first template parameter is an x::ptr to an object that's described by
//! create_managed_object().
//!
//! make_versioned_current() returns a reference to a subclass of
//! %currentObj<ptr_type, versionedptr<ptr_type> >, with overridden
//! update() and connection_update() methods that invoke functors that are
//! passed as parameters.
//!
//! The remaining template parameters are inferred, from the arguments.

template<typename ptr_type,
	 typename UpdateFunctor, typename ConnUpdateFunctor>
x::ref<currentObj<ptr_type, versionedptr<ptr_type> > >
inline make_versioned_current(//! Update functor.

			      //! Gets invoked after update().
			      //! The first parameter to the functor should be
			      //! declared as "const ptr_type &". The second
			      //! parameter is a bool.
			      //!
			      //! See currentBaseObj::update() for more
			      //! information.
			      UpdateFunctor && upd_funct,

			      //! Connection update functor.

			      //! Gets invoked after connection_update().
			      //! Takes a req_stat_t parameter.
			      //!
			      //! See currentBaseObj::connection_update() for
			      //! more information.
			      ConnUpdateFunctor && conn_upd_funct)
{
	return x::ref<versionedCurrentObj<ptr_type,
					  versionedptr<ptr_type>,
					  typename std::decay<UpdateFunctor>
					  ::type,
					  typename std::decay<ConnUpdateFunctor>
					  ::type> >
		::create(std::forward<UpdateFunctor>(upd_funct),
			 std::forward<ConnUpdateFunctor>(conn_upd_funct));
}

//! Construct a currentObj that uses a versionedptr, with notification functors.

//! An overload that supplies a do-nothing connection update functor. Use if
//! you only care about update notifications.

template<typename ptr_type,
	 typename UpdateFunctor>
x::ref<currentObj<ptr_type, versionedptr<ptr_type> > >
inline make_versioned_current(UpdateFunctor && upd_funct)
{
	return make_versioned_current
		<ptr_type>(std::forward<UpdateFunctor>(upd_funct),
			   []
			   (req_stat_t dummy)
			   {
			   });
}



STASHER_NAMESPACE_END

#endif

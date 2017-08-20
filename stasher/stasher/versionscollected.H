/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_versionscollected_H
#define stasher_versionscollected_H

#include <stasher/namespace.H>
#include <stasher/versionedptr.H>

#include <x/weakptr.H>
#include <list>
#include <type_traits>

STASHER_NAMESPACE_START

class versionscollectedBase;

//! A list of weak pointers to mcguffins of a collection of \ref versionedptr "versionedptr"s.

//! versioned_put(), versioned_put_from(), and versioned_put_fromiter() collect
//! the mcguffins of alleged current versions of the objects, and put them here.
//! This object is meant to be used by those functions only, which respect
//! the limitations of \ref versionedptr "versionedptr"'s mcguffins.

class versionscollectedObj : virtual public x::obj {

public:
	//! The collection of mcguffins from a collection of \ref versionedptr "versionedptr"s.

	std::list<x::weakptr<x::ptr<x::obj> > > versions;

	//! Constructor
	versionscollectedObj();

	//! Destructor
	~versionscollectedObj();

	//! Collect a version mcguffin from a versioned object.

	template<typename arg_type> void add(const arg_type &arg)
	{
		versions.emplace_back(arg.getversion());
	}

	//! Check if all the version mcguffins are still there

	//! Return \c true if all weakly-referenced objects in versions exist.
	//! If one or more are out of scope and have been destroyed, return
	//! \c false.
	//!
	//! Strong references to all the mcguffins get extracted, and put into
	//! the list that's passed as a parameter. A \c true return value
	//! implies that all strong references have been recovered. The
	//! passed container gets partially filled when \c false gets returned
	//! (until the first null weak pointer gets encountered, the container
	//! collects the strong references), and should be discarded.
	//!
	//! All strong references should be promptly discarded anyway, as
	//! required by \ref versionedptr "versionedptr".

	bool if_all_unchanged(//! Container for recovered mcguffins
			      std::list<x::ref<x::obj> > &versionsListRef);
};

//! A reference to version mcguffins of objects passed to versioned_put(), versioned_put_from(), and versioned_put_fromiter().

//! Used internally by versioned_put_from(), and versioned_put_fromiter(). Or,
//! construct it yourself, by hand, in a similar manner, and
//! invoke versioned_put() directly.
typedef x::ref<versionscollectedObj, versionscollectedBase> versionscollected;

//! A nullable pointer reference to collected version mcguffins

//! \see versionscollected.
typedef x::ptr<versionscollectedObj,
	       versionscollectedBase> versionscollectedptr;

//! Base class for references and pointers to versionscollectedObj.

//! Refer to this class as \c versionscollected::base or
//! \c versionscollected::base::base.

class versionscollectedBase : public x::ptrref_base {

public:

	//! Tail end of the variadic create_from()

	static versionscollected create_from();

	//! Retrieve the mcguffins from \ref versionedptr "versionedptr"s.

	//! create_from() collects version mcguffins from one or more
	//! \ref versionedptr "versionedptr"s.
	//!
	//! Each parameter may either be: a reference to a
	//! \ref versionedptr "versionedptr"; or something whose -> operator
	//! resolves to one. Each parameter's getversion() method gets invoked,
	//! and the results are collected into a
	//! \ref versionscollected "versionscollected" pointer reference.
	//!
	//! \note
	//! No locking gets done. It's your
	//! responsibility to suitably protected all
	//! \ref versionedptr "versionedptr"s passed here.
	//! When the \ref versionedptr "versionedptr"s come from multiple
	//! \ref currentObj "currentObj"s, always lock them in the same order,
	//! to avoid a deadlock.

	template<typename arg_type, typename ...remainingArgType>
	static inline versionscollected create_from(arg_type &&arg,
						    remainingArgType && ...more)
	{
		auto rest=create_from(std::forward<remainingArgType>(more)...);

		rest->add(std::forward<arg_type>(arg));
		return rest;
	}

	//! An alternate to create_from() that uses an input sequence.

	//! Instead of explicitly passing the collection of versioned pointers
	//! as parameters to create_from(), create_from_iter() takes them from
	//! an input sequence defined by some beginning iterator or an ending
	//! iterator.
	//!
	//! The iterator sequence here should iterate over either a
	//! \ref versionedptr "versionedptr" sequence, or something that
	//! implements a suitably similar getversion() method.
	//!
	//! It's your responsibility that this is thread safe,
	//! like create_from().

	template<typename iter_type>
	static versionscollected create_from_iter(//! Beginning iterator
						  iter_type b,

						  //! Ending iterator
						  iter_type e)
	{
		auto vers=create_from();

		while (b != e)
		{
			vers->add(*b);
			++b;
		}
		return vers;
	}
};

STASHER_NAMESPACE_END

#endif

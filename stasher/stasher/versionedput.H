/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_versionedput_H
#define stasher_versionedput_H

#include <stasher/namespace.H>
#include <stasher/versionscollected.H>
#include <stasher/client.H>
#include <stasher/manager.H>
#include <stasher/process_request.H>

#include <type_traits>

STASHER_NAMESPACE_START

//! Helper object used by versioned_put_request().

//! Holds the functor and the results of the processed put request.
//! The destroyed() callback invoked the functor.

template<typename Functor>
class versionedDestroyCallbackObj : public x::destroyCallbackObj {

	//! The saved function
	typename std::enable_if<
		!std::is_lvalue_reference<Functor>::value &&
	!std::is_rvalue_reference<Functor>::value, Functor>::type f;

	//! Results of the put request.
	putresults res;

public:
	//! Constructor
	versionedDestroyCallbackObj(const Functor &fArg,
				    const putresults &resArg)
		: f(fArg), res(resArg)
	{
	}

	//! Destructor
	~versionedDestroyCallbackObj() noexcept {
	}

	//! Invoke the original functor
	void destroyed() noexcept
	{
		try {
			f(res);
		} catch (const x::exception &e)
		{
			managerObj::handle_exception(e);
		}
	}
};

//! Update one or more managed objects. Upon a uuid mismatch, wait until at least one of them gets updated, before taking the next step.

//! A put transaction may fail due to a uuid mismatch because one of the objects
//! in the repository have been updated. However, it's not guaranteed that
//! the managed object here has been updated, because the manager may still be
//! waiting for the updated value of the object. The sequence of the events
//! would be:
//!
//! - a put transaction gets sent into the object repository.
//!
//! - one of the objects gets updated.
//!
//! - this client's manager gets notified, and it sends a request for
//! the object's updated value.
//!
//! - the put transaction gets kicked back to this client, with a
//! \ref req_stat_t "req_rejected_stat".
//!
//! At this point the manager didn't get the updated value, it's on the way
//! but not here yet, and needs to wait.
//!
//! versioned_put_request() simplifies the logic for handling this situation.
//! versioned_put_request() takes the following arguments:
//!
//! - a client connection handle.
//!
//! - a prepared transaction to put into the repository.
//!
//! - a functor that takes a \ref putresults as a parameter.
//!
//! - a \ref versionscollected "versionscollected" handle.
//!
//! The general approach for using versioned_put_request():
//!
//! - the objects being updated here are kept in \ref versionedptr "versionedptr"s.
//!
//! - take whatever locking steps are needed to lock the
//! \ref versionedptr "versionedptr"s.
//!
//! - collect all the version mcguffins from the
//! \ref versionedptr "versionedptr"s; typically simply by calling
//! \ref versionscollectedBase::create_from "versionscollected::base::create_from"(), or
//! \ref versionscollectedBase::create_from_iter "versionscollected::base::create_from_iter"(),
//! which discuss the locking issues in more detail.
//!
//! - prepare the transaction, and release any locks.
//!
//! - invoke versioned_put_request().
//!
//! This function has the following results:
//!
//! - invoke \ref clientObj::put_request "client->put_request()". This
//! returns immediately, while all the remaining processing that follows gets
//! taken care of independently.
//!
//! - when the request gets processed, if \ref putresults "putresults"'s status
//! is anything other than
//! \ref req_stat_t "req_rejected_stat", invoke the functor passing
//! it the \ref putresults "putresults" as a parameter.
//!
//! - for a \c req_rejected_stat return value, the functor gets invoked only
//! after one of the collected version mcguffins goes out of scope and
//! gets destroyed, indicating that one (or more) objects that went into
//! the transaction has been updated. If this has already happened when
//! the put transaction transaction got processed, the functor gets invoked
//! immediately, otherwise, whenever some object gets updated.
//!
//! This means that when the functor gets invoked with a \c req_rejected_stat,
//! it's guaranteed that one or more of the objects in the transaction has
//! been updated already. The functor can decide whether or not to try another
//! transaction or do something else. Its up to the functor to decide what to
//! do for other statuses.
//!
//! \note
//! The functor gets invoked in the context of the client connection thread.
//! It should not perform extensive processing. Ideally, it should just send
//! a message, with the received \ref putresults "puresults",
//!  to some other thread to deal with. Thrown exceptions get logged.

template<typename Functor>
void versioned_put_request(//! Client connection handle
			   const client &cl,
			   //! A transaction that updates one or more objects that are \ref create_managed_object "managed" in a \ref versionedptr "versionedptr".

			   const client::base::transaction &tr,

			   //! A functor that received a \ref putresults "putresults" of the transaction
			   const Functor &f_arg,

			   //! The version mcguffins of the objects that went into the transaction.
			   const versionscollected &vers_arg)
{
	Functor f=f_arg;
	versionscollected vers=vers_arg;

	process_request(cl->put_request(tr),
			[f, vers] (const putresults &res)
			{
				typedef typename std::enable_if<
					!std::is_lvalue_reference<Functor>::value &&

					!std::is_rvalue_reference<Functor>::value
					>::type cannot_be_ref_type __attribute__((unused));

				auto cb=x::ref<versionedDestroyCallbackObj
					       <Functor> >::create(f, res);

				std::list<x::ref<x::obj> > versions;

				if (res->status != req_rejected_stat ||
				    !vers->if_all_unchanged(versions))
				{
					cb->destroyed();
					return;
				}

				cb->onAnyDestroyed(versions.begin(),
						   versions.end());
			});
}

//! Convenience function for invoking versioned_put_request().

//! Forwards the objects passed as parameters to
//! \ref versionscollectedBase::create_from "versionscollected::base::create_from"(),
//! then forwards the constructed \ref versionscollected "versioncollected"
//! to versioned_put_request().

template<typename Functor, typename ...fromObjects>
void versioned_put_request_from(//! Client connection handle
				const client &cl,
				//! A transaction that updates one or more objects that are \ref create_managed_object "managed" in a \ref versionedptr "versionedptr".
				const client::base::transaction &tr,
				//! A functor that received a \ref putresults "putresults" of the transaction
				const Functor &f,

				//! Parameters forwarded to \ref versionscollectedBase::create_from "versionscollected::base::create_from"().
				fromObjects && ...objects)
{
	versioned_put_request(cl, tr, f,
		      versionscollected::base
		      ::create_from(std::forward<fromObjects>(objects)...));
}

//! Convenience function for invoking versioned_put_request().

//! Forwards the objects passed as parameters to
//! \ref versionscollectedBase::create_from_iter "versionscollected::base::create_from_iter"(),
//! then forwards the constructed \ref versionscollected "versioncollected"
//! to versioned_put_request().

template<typename Functor, typename iter_type>
void versioned_put_request_fromiter(//! Client connection handle
				    const client &cl,
				    //! A transaction that updates one or more objects that are \ref create_managed_object "managed" in a \ref versionedptr "versionedptr".
				    const client::base::transaction &tr,
				    //! A functor that received a \ref putresults "putresults" of the transaction
				    const Functor &f,
				    //! Beginning iterator forwarded to \ref versionscollectedBase::create_from_iter "versionscollected::base::create_from_iter"().
				    iter_type b,
				    //! Ending iterator forwarded to \ref versionscollectedBase::create_from_iter "versionscollected::base::create_from_iter"().
				    iter_type e)
{
	versioned_put_request(cl, tr, f,
			      versionscollected::base::create_from_iter(b, e));
}

//! Update one or more managed objects. Upon a uuid mismatch, wait until at least one of them gets updated, before returning.

//! This is the synchronous version of versioned_put_request(). See
//! versioned_put_request() for more information.
//!
//! versioned_put() waits for the transaction to get processed, and returns
//! the results of the transaction. In the event of a \c req_rejected_stat,
//! versioned_put() waits until at least one of the objects gets updated, if
//! none of them are, before returning.

putresults versioned_put(//! Client connection handle
			 const client &cl,
			 //! A transaction that updates one or more objects that are \ref create_managed_object "managed" in a \ref versionedptr "versionedptr".

			 const client::base::transaction &tr,

			 //! The version mcguffins of the objects that went into the transaction.
			 const versionscollected &vers);

STASHER_NAMESPACE_END

#endif

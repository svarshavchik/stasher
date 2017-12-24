/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_process_request_H
#define stasher_process_request_H

#include <stasher/namespace.H>
#include <stasher/client.H>

STASHER_NAMESPACE_START

//! Process a client request.

//! \code
//! process_request(client->put_request(transaction),
//!                 [] (const putresults &res) { ... });
//! \endcode
//!
//! The first parameter is the return value from a call to any _request()
//! method of a client object. The second parameter is a lambda function
//! (or a reasonable facsimile that implements the appropriate () operator)
//! that serves as a callback.
//!
//! process_request() makes arrangements to invoke the callback function
//! when the request gets processed. It gets the request mcguffin and
//! attaches a destructor callback that passes the results of the request
//! to the callback.

template<typename req_type, typename cb_type>
void process_request(const std::pair<req_type, client::base::request> &req,
		     cb_type &&cbArg)
{
	req_type reqmsg=req.first;

	cb_type cb_cpy(std::move(cbArg));

	req.second->mcguffin()
		->ondestroy([cb_cpy, reqmsg]
			    {
				    // A compilation error here indicates
				    // that the functor/lambda is passed
				    // by reference, or the request type is
				    // a reference. They should not be.
				    typedef typename std::enable_if<
					    !std::is_rvalue_reference<cb_type>::value &&

					    !std::is_lvalue_reference<cb_type>::value
					    >::type cannot_be_ref_type __attribute__((unused));
				    typedef typename std::enable_if<
					    !std::is_rvalue_reference<req_type>::value &&

					    !std::is_lvalue_reference<req_type>::value
					    >::type cannot_be_ref_type_too __attribute__((unused));

				    cb_cpy(reqmsg->getmsg());
			    });
}

STASHER_NAMESPACE_END

#endif

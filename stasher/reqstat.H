/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_reqstat_H
#define stasher_reqstat_H

#include <stasher/namespace.H>

#include <x/to_string.H>
#include <x/serialization.H>

STASHER_NAMESPACE_START

//! Request status enumeration

typedef enum { req_processed_stat, //!< The requested operation processed succesfully

	       req_rejected_stat, //!< Transaction rejected due to uuid mismatch
	       req_badname_stat,  //!< Invalid object name
	       req_dupeobj_stat,  //!< Duplicate object name in transaction
	       req_enomem_stat,   //!< Repository out of disk space
	       req_toomany_stat,  //!< Too many objects in the transaction
	       req_name2big_stat, //!< Object name too big
	       req_2large_stat,   //!< Object too large, not passed as a plain file, or aggregate object size too large
	       req_disconnected_stat, //!< A connection request failed, or an existing connection to the server failed
	       req_eperm_stat,	  //!< Permission denied, cannot update object
	       req_failed_stat    //!< Other, internal error
} req_stat_t;

STASHER_NAMESPACE_END

namespace x {
	template<>
	std::string to_string(const STASHER_NAMESPACE::req_stat_t &,
			      const const_locale &localeRef);
};

LIBCXX_SERIALIZE_ENUMERATION(STASHER_NAMESPACE::req_stat_t,char);

#endif

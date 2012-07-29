/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "stasher/reqstat.H"

template<>
std::string x::tostring(const STASHER_NAMESPACE::req_stat_t &value,
			const const_locale &dummy)

{
	const char *p;

	switch (value) {
	case STASHER_NAMESPACE::req_processed_stat:
		p="Transaction/request processed";
		break;
	case STASHER_NAMESPACE::req_rejected_stat:
		p="Collision detected - object uuid mismatch";
		break;
	case STASHER_NAMESPACE::req_badname_stat:
		p="Object name is not valid";
		break;
	case STASHER_NAMESPACE::req_dupeobj_stat:
		p="Duplicate object name in transaction";
		break;
	case STASHER_NAMESPACE::req_enomem_stat:
		p="Repository out of disk space";
		break;
	case STASHER_NAMESPACE::req_toomany_stat:
		p="Number of objects in transaction exceeds server's limits";
		break;
	case STASHER_NAMESPACE::req_name2big_stat:
		p="Object name exceeds maximum size";
		break;
	case STASHER_NAMESPACE::req_2large_stat:
		p="Transaction content exceeds server's limits, or specified as a non-plain file";
		break;
	case STASHER_NAMESPACE::req_disconnected_stat:
		p="Connection to the server failed";
		break;
	case STASHER_NAMESPACE::req_eperm_stat:
		p="Permission denied";
		break;
	case STASHER_NAMESPACE::req_failed_stat:
		p="Transaction failed due to an internal error, no further information available";
		break;
	default:
		p="Unknown error code";
		break;
	}

	return p;
}

template<>
std::wstring x::towstring(const STASHER_NAMESPACE::req_stat_t &value,
			  const const_locale &localeRef)

{
	return x::towstring(x::tostring(value, localeRef), localeRef);
}


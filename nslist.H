/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef nslist_H
#define nslist_H

#include "nsmap.H"

//! List of defined namespaces

struct nslist {

public:
	//! Read/write namespaces
	std::vector<nsmap> rw;

	//! Read-only namespaces
	std::vector<nsmap> ro;

	//! Serialization function

	template<typename ptr_type, typename iter_type>
	static void serialize(ptr_type ptr, iter_type &iter)
	{
		iter(ptr->rw);
		iter(ptr->ro);
	}
};

//! The namespace list object

#define NSLISTOBJECT "etc/namespaces"

#endif

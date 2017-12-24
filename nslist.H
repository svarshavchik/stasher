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

	template<typename iter_type> void serialize(iter_type &iter)

	{
		iter(rw);
		iter(ro);
	}
};

//! The namespace list object

#define NSLISTOBJECT "etc/namespaces"

#endif

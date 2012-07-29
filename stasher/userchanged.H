/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef userchanged_H
#define userchanged_H

#include "stasher/namespace.H"
#include <x/exception.H>
#include <string>

STASHER_NAMESPACE_START

//! Notification from server to client that an object has been changed

class userchanged {

public:
	//! Object name

	std::string objname;

	//! Directory's object name

	//! If objname is a directory, and the notification gets sent
	//! as a result of the contents of the directory being changed,
	//! this is the added, modified, or removed object in the
	//! directory.

	std::string suffix;

	//! Default constructor

	userchanged();

	//! Constructor

	userchanged(const std::string &objnameArg,
		    const std::string &suffixArg);

	//! Destructor
	~userchanged() noexcept;

	//! Serialization function
	template<typename iter_type>
	void serialize(iter_type &iter)
	{
		iter(objname);
		iter(suffix);
	}
};

STASHER_NAMESPACE_END

#endif

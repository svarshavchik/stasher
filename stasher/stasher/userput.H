/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_userput_H
#define stasher_userput_H

#include <x/exception.H>
#include <x/fd.H>

#include <stasher/namespace.H>
#include <stasher/puttransaction.H>

class localconnectionObj;

STASHER_NAMESPACE_START

class fdobjwriterthreadObj;
class userinit;

//! Helper classes for serializing and deserializing a PUT transaction

//! For serialization, the constructor takes a reference to
//! a prepared puttransactionObj. The write() method writes the object
//! to a fdobjwriterthreadObj, followed by the contents of each new or updated
//! object in the transaction. For the contents specified as a string,
//! write() writes a puttransactionObj::content_str. For the contents specified
//! as a file descript, write() invokes sendfd(). The serialization function
//! simply invokes puttransactionObj's serialization function. [USERPUTWRITE]
//!
//! \par Deserialization
//!
//! When constructed for deserialization purposes, an empty puttransactionObj
//! gets constructed, then the metadata gets deserialized. After deserialization
//! proceed as follows:
//! 
//! Construct a deserializedObj object, passing it the transaction object
//! that was deserialized.
//!
//! If deserializedObj::completed() returns false, wait until a
//! puttransactionObj::content_str was deserialized, or a socket was received
//! over the file descriptor, invoke received(), then check completed() again.
//!
//! When deserializedObj::completed() returns true, check errflag and
//! errmessage.
//! If errflag is false, tran contains a repository transaction, and
//! diskspace reserved space for it [USERPUTDESER].
//!
//! An object whose value is comes from the file descriptor has the file
//! descriptor sent over the server connection socket, directly.
//! The file descriptor must be a plain file.
//!
//! An object whose value is specified literally, if the value exceeds the
//! server-defined chunk size, the value gets automatically split into multiple
//! messages [USERPUTSPLIT].

class userput {

public:

	//! The transaction metadata

	x::ptr<puttransactionObj> tran;

	//! Requesting uuid;
	x::uuid uuid;

	//! Object contents in buffers that exceed this size get broken up
	size_t chunksize;

	//! Constructor for serializing
	userput(const x::ptr<puttransactionObj> &tranArg,
		const userinit &limits);

	//! Constructor for deserialization

	userput(const localconnectionObj &conn);

	//! Constructor for deserialization
	userput(const userinit &limits);

	//! Destructor
	~userput() noexcept
	{
	}

	//! The serialization function

	//! This is just a wrapper for the transaction object's serialization
	//! function.

	template<typename iter_type>
	void serialize(iter_type &iter)
	{
		iter(uuid);
		tran->serialize(iter);
	}

	//! Write this transaction into a file descriptor

	void write(const x::ptr<fdobjwriterthreadObj> &wthread)
;

	class deserializedObj;
};

STASHER_NAMESPACE_END

#endif

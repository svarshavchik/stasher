/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef newtran_H
#define newtran_H

#include "tranmeta.H"
#include "newtranfwd.H"
#include "tobjrepofwd.H"

#include <x/logger.H>

class transerializer;

//! A new object transaction

//! This class should not be instantiated directly, but rather by invoking
//! tobjrepoObj::newtransaction().
//!
//! tobjrepoObj::newtransaction() returns a reference to this object.
//! Invoke newobj(), updobj(), and delobj() to add objects to the transaction,
//! then finalize() once the transaction is complete.

class newtranObj : virtual public x::obj {

	//! The logger

	LOG_CLASS_SCOPE;

	//! New transaction uuid

	x::uuid uuid;

	//! Transaction metadata
	tranmeta meta;

	//! My transaction repository
        tobjrepo repo;

	//! This transaction was finalized

	bool finalized;

public:
	friend class tobjrepoObj;
	friend class transerializer;

	//! Constructor
	newtranObj(const tobjrepo &repoArg,
		   const x::uuid &uuidArg=x::uuid());

	//! Destructor
	~newtranObj() noexcept;

	//! Return transaction options

	std::map<std::string, std::string> &getOptions() noexcept
	{
		return meta.opts;
	}

	//! A new object

	//! Returns a file descriptor. Write the object's value to the
	//! file descriptor, then close it.
	x::fd newobj(//! New object
		     const std::string &objname);

	//! Update an existing object

	//! Returns a file descriptor. Write the object's new value to the
	//! file descriptor, then close it.

	x::fd updobj(//! Object to update
		     const std::string &objname,

		     //! Object's current serial value
		     const x::uuid &sv);

	//! Delete an object
	void  delobj(//! Object to delete
		     const std::string &objname,

		     //! Object's current serial value
		     const x::uuid &sv);

	//! Finalize this transaction

	//! After finalize() returns, this new transaction is no longer
	//! used.
	//!
	//! \return this transaction's unique identifier, which is the
	//! serial value of new and updated objects in this transaction,
	//! if it gets commited.

	x::uuid finalize();

	//! Container for enumerating sizes of new objects

	typedef std::map<std::string, uint64_t> objsizes_t;

	//! Retrieve a list of new or updated objects, and their sizes

	//! This may be done just before the transaction is finalized.
	//!

	void objsizes(objsizes_t &objsizeret) const;
};

#endif

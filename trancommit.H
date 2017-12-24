/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef trancommit_H
#define trancommit_H

#include "tran.H"
#include "tobjrepo.H"
#include "trancommitfwd.H"

//! A handle for a transaction to be commited

//! Not instantiated directly. Instantiated by tobjrepoObj::begin_commit().
//!
//! \par Checking if the transaction is ready [READY]
//!
//! ready() returns an indication whether the transaction is ready to be
//! commited. If ready() returns false, wait on the event file descriptor that
//! was passed to tranObj::begin_commit(), then call ready() again, repeating
//! the process until ready() returns true.
//!
//! \par Verifying a transaction [VERIFY]
//!
//! Invoke verify() to check if the transaction specifies matching serial
//! values for all objects updated or removed by the transaction.
//!
//! \par Committing the transaction [TRANCOMMIT]
//!
//! Invoke commit() to commit the transaction. After commit() returns, this
//! transaction object is a zombie and serves no further purpose.
//!
//! Commit hardlink the temporary .t file to .c, proceeds to install and
//! delete objects, as specified in the transaction, then removes the .c file
//! hardlink. Note that the .t file remains. Invoke tobjrepoObj::cancel() to
//! remove the .t file.
//!
//! If the transaction metaoption tobjrepoObj::node_opt is set (this is always
//! the case except in some regression tests) commit() also installs
//! a dummy 0-length object, "etc/done/{node}/{uuid}" to mark the transaction
//! as being commited in the repository [MARKDONE].
//! This gets removed by cancel().

class trancommitObj : private tranObj, virtual public x::obj {

	//! My repository
	tobjrepoptr repo;

	//! My lock on the objects
	tobjrepoObj::lockentryptr_t lock;

	//! My read lock on the global commit lock

	tobjrepoObj::commitlockptr_t commitlock;

public:
	friend class tobjrepoObj;

	//! Constructor
	trancommitObj(const x::uuid &uuidArg,
		      const tobjrepo &repoArg);
	~trancommitObj();

	//! Read-only access to options

	const std::map<std::string, std::string> &getOptions() const noexcept
	{
		return opts;
	}

	//! Whether the transaction is ready to be commited

	bool ready()
	{
		return lock->locked() && commitlock->locked();
	}

	//! Verify matching serial values

	bool verify() const;

	//! Commit the transaction

	void commit();
};

#endif

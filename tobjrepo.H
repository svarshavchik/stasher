/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef tobjrepo_H
#define tobjrepo_H

#include "objrepo.H"
#include "trandistuuid.H"

#include "stasher/namespace.H"
#include "stasher/reqstat.H"

#include "tobjrepofwd.H"

#include <x/uuid.H>
#include <x/fd.H>
#include <x/eventfd.H>
#include <x/lockpool.H>
#include <x/mpobj.H>
#include <set>

#include "newtranfwd.H"
#include "trancommitfwd.H"
#include "tranfwd.H"

class tranmeta;
class transerializer;
class trandistributorObj;

//! A transaction object repository

//! Derives from objrepoObj, and implements an object repository where
//! objects get updated in an atomic transaction.
//!
//! \par Inherited methods [INHERITED]
//!
//! obj_begin() and obj_end(), enumerating the existing objects, are inherited
//! from the superclass, as well as the notification framework.
//!
//! \par Creating transactions [CREATING]
//!
//! newtransaction() returns a reference to an object that represents a new
//! transaction. Populate the transaction using its methods, then invoke its
//! %finalize() method
//! to return the transaction's identifier, which is the same as
//! the serial values of all objects added or updated by the transaction
//! (if it gets commited). After %finalize(), the new transaction object is no
//! longer valid.
//!
//! \par Temporary filename naming conventions [FILES]
//!
//! - {uuid}.t contains the serialized transaction information
//!
//! - {uuid}.# where "#" is numeric -- values of objects in the transaction
//!
//! - {uuid}.c transaction in the process of being commited
//!
//! - {uuid}.x transaction in the process of being cancelled
//!
//! \par Cleanup [CLEANUP]
//!
//! When a tobjrepoObj gets instantiated, .c and .x files get recommited or
//! cancelled. Then, any temporary files other than the remaining .t and #
//! files get purged.
//!
//! \par Corrupted transaction file recovery [FAILCORRUPTED]
//!
//! When a tobjrepoObj gets instantiated, if a .c or .x file cannot be
//! recommitted or canceled, or the .t file does not contain a parsable
//! transaction, it gets removed
//!
//! \par Cancelling transactions [CANCEL]
//!
//! cancel() takes a transaction identifier, and removes the transaction.
//! First, the .t file gets renamed to .x, then all the corresponding
//! temporary files with contents of new/updated objects are removed, followed
//! by the .x file itself.
//!
//! cancel() should also be invoked after the transaction gets commited, to
//! remove the .t file, and the "etc/done/{node}/{uuid}" object from the
//! repository [CANCELMARKDONE].
//!
//! \par Beginning a commit [COMMIT]
//!
//! begin_commit() takes a transaction identifier and an event file descriptor,
//! and returns a handle for committing the transaction. Committing means
//! installing, updating, or deleting objects in the transaction.
//!
//! begin_commit() does not result in the transaction getting commited, it's
//! the first step in the process.
//!
//! \par Global commit lock [GLOBALLOCK]
//!
//! Invoking commitlock() returns a commitlock_t object. When
//! commitlock_t->locked() returns true, all commits are blocked (the %ready()
//! calls to a commit handle will return false until commitlock_t goes out of
//! scope.
//!
//! If commitlock_t->locked() returns false, wait on the event file descriptor
//! that was passed to commitlock(), then try again.
//!
//! \par Manual object lock [OBJECTLOCK]
//!
//! lock() creates a handle for a manual lock on a set of object names.
//! lock() returns a lockentry_t reference. Invoking lockentry_t->locked()
//! returns true when the lock has been succesfully acquired. If it returns
//! false, wait on the event file descriptor that was passed to lock(), then
//! try again.
//!
//! After the lock is acquired, any transactions that involve the locked
//! objects will remain blocked (their locked() method will return false)
//! until the lockentry_t reference goes out of scope.
//!
//! \par Retrieving objects [VALUES]
//!
//! values_t and values() are inherited from the superclass. The objects
//! should be locked first.
//!
//! \par Marking transactions as done [MARKDONESTAT]
//!
//! Objects named {done_hier}/{source}/{uuid}, where "done_hier" is given
//! by tobjrepoObj::done_hier, "source" is the value of the transactions'
//! tobjrepoObj::node_opt meta option, and "uuid" is the transaction's uuid
//! get created when the transaction gets commited. These objects also
//! get created to mark the transaction as failed.
//!
//! These objects contain a single \c req_stat_t enumeration value, giving
//! the status of the transaction. commit() creates this object containing
//! \c req_processed_stat, mark_done() manually creates this object,
//! and get_tran_stat() retrieves the transaction status value,
//! cancel_done() manually removes the done object (this is also done as part
//! of cancel).
//!
//! \par The failed list
//!
//! A higher-level class makes a determination that a transaction cannot
//! be created in the repository because there's no space for it.
//!
//! failedlist_insert() adds the transaction uuid to the failed list, together
//! with its noted source, and its error code.
//!
//! enumerate() goes through the uncommited transactions in the repository,
//! and invokes a callback for each transaction, giving back the transaction
//! uuid, its source, from the transactions tobjrepoObj::node_opt meta option,
//! and its status. This list also includes all the transactions on the failed
//! list [TRANENUMERATE].
//!
//! cancel() checks if the passed uuid is on the failed list; if so the
//! uuid is removed from the failed list, and cancel() takes no action
//! [FAILCANCEL].
//!
//! mark_done() checks if the passed uuid is on the failed list; if so
//! mark_done() takes no action and terminates [FAILMARKDONE].


class tobjrepoObj : private objrepoObj, virtual public x::obj {

public:
	friend class newtranObj;
	friend class trancommitObj;
	friend class transerializer;
	friend class trandistributorObj;

	//! Construct and bind to an object repository directory
	tobjrepoObj(const std::string &directoryArg);

	//! Destructor
	~tobjrepoObj();

	//! "NODE"

	//! A transaction option that identifies the transaction's originating
	//! node.
	static const char node_opt[];

	//! Create a new transaction object
	newtran newtransaction();

	//! Create a new transaction object with a specific uuid

	//! This is used when copying reposito
	newtran newtransaction(const x::uuid &uuidArg);


	//! Cancel a transaction
	void cancel(const x::uuid &uuidArg);

	//! Begin the commit process

	trancommit begin_commit(const x::uuid &uuidArg,
					   const x::eventfd &fdArg);


	//! Commit the transaction directly, bypassing the locking

	//! This call is used when copying a repository or when the master
	//! order this slave node to commit.

	void commit_nolock(const x::uuid &uuidArg);


	//! Alias definition

	typedef objrepoObj::obj_iter_t obj_iter_t;

	//! Alias definition
	typedef objrepoObj::dir_iter_t dir_iter_t;

	using objrepoObj::obj_begin;
	using objrepoObj::obj_end;
	using objrepoObj::dir_begin;
	using objrepoObj::dir_end;
	using objrepoObj::notifierObj;
	using objrepoObj::notifier;
	using objrepoObj::installNotifier;
	using objrepoObj::obj_name_len;
	using objrepoObj::obj_name_create;
	using objrepoObj::directory;
	//! Callback for enumeration of finalized transaction

	class finalized_cb {

	public:
		finalized_cb() noexcept {}
		~finalized_cb() {}

		//! Callback invoked for each enumerated uncommited transaction
		virtual void operator()(//! Transaction ID
					const x::uuid &uuid,

					//! Transaction's receipt status and source
					const dist_received_status_t &status)
=0;
	};

	//! Enumerate uncommited transactions

	void enumerate(//! Callback that receives enumerated uuids.
		       finalized_cb &cbArg);

private:
	//! The logger class
	LOG_CLASS_SCOPE;

	//! The type which implements transaction locking

	typedef x::multilockpool<std::string>::lockpool_t lockpool_t;

public:
	//! The type of a transaction lock
	typedef lockpool_t::obj_type::lockentry lockentry_t;

	//! A nullable pointer to the transaction lock object.
	typedef lockpool_t::obj_type::lockentryptr lockentryptr_t;

private:
	//! Transaction lock pool

	lockpool_t lockpool;

	//! Create a temporary file
	x::fd tmp_create(const x::uuid &uuid, size_t objnum);


	//! Reopen a temporary file
	x::fd tmp_reopen(const x::uuid &uuid, size_t objnum);


	//! Remove a temporary file
	void tmp_remove(const x::uuid &uuid, size_t objnum);

public:
	//! Override the uuid of a tmp file.

	//! This is used when copying objects between repositories.

	static void tmp_set_uuid(const x::fd &tmp_fd, const x::uuid &uuid);

private:
	//! Save transaction file
	void finalize(const newtran &tran);

	//! Load an existing transaction from a file
	void parse(tranmeta &tranArg,
		   const x::fd &fdArg);

	//! Open the temporary file with the serialized transaction info, the transaction uuid is now locked.

	x::fd open_tran_uuid_locked(const x::uuid &uuidArg);

	//! Internal cancellation implementation
	void cancel_uuid_locked(const tran &tranArg);


	//! Rename transaction file and commit

	void commit(const x::uuid &uuidArg, const tranmeta &tranArg,
		    const x::ptr<x::obj> &lock);


	//! Global commit lock type

	typedef x::mutexpool<> commitlockpool_t;

	//! Global commit lock

	commitlockpool_t commitlockpool;

public:

	//! Transactions marked as done

	//! This is "etc/done". Transactions, once committed, create a dummy
	//! object named "etc/done/{source}/{uuid}", where {source} is
	//! the contents of the transaction's "node_opt" option, and {uuid}
	//! is the transaction's uuid. This way, if the distributing node goes
	//! down, and in the meantime the cluster's master commits the
	//! transaction, once the distributing node rejoins the cluster, it
	//! resynchronized its copy of the object repository, and gets notified
	//! that the transaction was processed as part of the resynchronization.
	//!
	//! If the master rejects the transaction, it also installs "etc/done"
	//! for the same reason.

	static const char done_hier[];

	//! The size of done_hier
	static const size_t done_hier_l;

	//! Return object hierarchy for done files from a given source

	//! This returns, basically, done_hier + "/" + source
	//!
	std::string source_done_hier(const std::string &source);


	//! Mark a transaction as "done"

	//! This creates and installs an object named done_hier/{source}/{uuid}
	//! containing the transaction's status

	void mark_done(//! Transaction ID
		       const x::uuid &uuid,

		       //! The value of transaction's node_opt option

		       const std::string &source,

		       //! The transaction's status
		       STASHER_NAMESPACE::req_stat_t value,

		       //! Passthrough argument from commit()
		       const x::ptr<x::obj> &lockArg);

	//! Lock out all other access to a transaction

	//! Should be instantiated on a stack, or a separate reference must
	//! remain on the transaction repo object while this lock is in scope.
	//!
	//! Acquires and holds for a lock
	//! upon construction. The destructor releases the lock.

	class uuidlock {

		//! The repository
		tobjrepoObj *p;

		//! The acquired lock
		std::set<x::uuid>::iterator lockiter;

	public:
		//! Constructor waits and acquires a lock for the given ID.
		uuidlock(//! Pointer to the repository object.

			 //! If NULL, this is a no-op.
			 //!
			 tobjrepoObj *pArg,

			 //! Which uuid to lock
			 const x::uuid &uuidArg);

		//! The destructor releases the lock
		~uuidlock();
	};

	friend class uuidlock;
private:

	//! The container for transactions that are cancelled, marked, or commited
	typedef std::set<x::uuid> locked_uuids_t;

	//! The mutex-protected container for locked_uuids_t

	typedef x::mpcobj<locked_uuids_t> locked_uuids_container_t;

	//! The transactions that are cancelled, marked, or commited

	locked_uuids_container_t locked_uuids;

	//! The transaction ID is locked in, so now proceed with committing it

	void commit_uuid_locked(const x::uuid &uuidArg, const tranmeta &tranArg,
				const x::ptr<x::obj> &lock);


	//! The transaction ID is locked in, so now proceed with committing it

	void commit_uuid_locked(const std::string &uuidArg,
				const tranmeta &tranArg,
				const x::ptr<x::obj> &lock);


	//! The transaction ID is locked in, so now proceed with marking it as done

	void mark_done_uuid_locked(//! Transaction ID
				   const x::uuid &uuid,

				   //! The value of transaction's node_opt option

				   const std::string &source,

				   //! The transaction's status
				   STASHER_NAMESPACE::req_stat_t value,

				   //! Passthrough argument from commit()
				   const x::ptr<x::obj> &lockArg);


public:
	//! Manually remove a done object

	//! This is done at startup.
	//!
	void cancel_done(const std::string &source,
			 const x::uuid &uuid);

	//! Cancel all done objects from a given source.

	//! This may also be done at startup.
	//!
	void cancel_done(const std::string &source);

	//! Open the object that recorded the transaction's status

	//! Returns an empty file descriptor reference if the transaction
	//! status file does not exist

	x::fdptr open_tran_stat(const std::string &source,
				const x::uuid &uuidArg);


	//! Retrieve the transaction's status

	//! returns req_failed_stat if the transaction's status is not known.
	STASHER_NAMESPACE::req_stat_t get_tran_stat(const std::string &source,
						     const x::uuid &uuidArg);


	//! Global commit lock type

	typedef commitlockpool_t::base::lockentry commitlock_t;

	//! Nullable pointer commit lock type

	typedef commitlockpool_t::base::lockentryptr commitlockptr_t;

	//! Write lock acquisition attempt

	commitlock_t commitlock(const x::eventfd &fdArg);

	//! A manual lock on a set of objects

	lockentry_t lock(//! Names of objects
			 const std::set<std::string> &objects,

			 //! Event file descriptor
			 const x::eventfd &fdArg);


	//! Export values_t

	typedef objrepoObj::values_t values_t;

	using objrepoObj::values;


private:
	//! A container used to hold failedlist
	typedef std::map<x::uuid, dist_received_status_t> failedlist_t;

	//! A mutex-protected container for failedlist_t

	typedef x::mpobj<failedlist_t> failedlist_container_t;

	//! Transactions that we failed to receive

	//! These transactions are not in the repository, they are tracked
	//! in memory. This list is expected to be rather short. The maximum
	//! number of distributed transactions per node is limited, and
	//! as soon as the transaction hits all nodes, the master is expected
	//! to cancel them. The contents of the map are: the name of the
	//! originating node, and the failure code.
	//!
	//! The failed list is maintained by the distributor thread

	failedlist_container_t failedlist;

public:
	//! Add a new entry to the failed list

	void failedlist_insert(//! Transaction identifier
			       const x::uuid &uuidArg,

			       //! Status and source
			       const dist_received_status_t &statusArg);

};

#endif

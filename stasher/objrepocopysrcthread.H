/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objrepocopysrcthread_H
#define objrepocopysrcthread_H

#undef libx_autogen_suppress_inner

#include "objrepocopysrcinterface.H"
#include "objrepocopydstinterfacefwd.H"
#include "tobjrepo.H"
#include "batonfwd.H"
#include "threadmgr.H"
#include <x/threadmsgdispatcher.H>
#include <x/weakptr.H>
#include <x/logger.H>

//! Execution thread implementation of the source side of a repo copy

//! Pass a reference to the destination side's interface for a repository copy
//! to construct a copycomplete handle, then start a thread using
//! a reference to the source, a baton for the source repository, if present,
//! the constructed copycomplete object,
//! and a mcguffin that the execution thread keeps a reference on, while
//! it's running.
//!
//! The copycomplete handle returns an indication whether the repository copy
//! operation is completed.
//! copycompleteObj::completed() indicates whether the repository copy operation
//! is in progress. If false, wait on the event file descriptor, and check
//! again, until copycompleteObj::completed() returns true. [COMPLETE]
//!
//! The execution thread releases its reference to the mcguffin
//! argument that was passed to run(). Wait for the mcguffin to be destroyed
//! to be notified when the sync operation is complete. [COPYSRCMCGUFFIN].
//!
//! Once the sync operation is complete, copycompleteObj::success() indicates
//! that the PRIMARY copy operation has been completed succesfully. As soon
//! as repository copy operation starts, any changes commited to the repository
//! asynchronously will be replicated to the destination repository
//! concurrently with the copy operation. The replication occurs in the
//! context of the thread that's updates the repository.
//!
//! Replication continues even after the primary copy operation is completed,
//! [REPLICATION] keeping the destination repository up to date. To complete
//! the repository copy, obtain a commit lock on the object repository, which
//! stops all updates, then invoke copycompleteObj::release(), which sends the
//! final message to the destination repository's copy thread. After release()
//! returns, both repositories may be considered identical, and the commit lock
//! can be released [IDENTICAL].
//!
//! Invoke event() to process messages received from the destination
//! repository's copy thread. The messages are:
//! objrepocopy::slavelist, objrepocopy::slavelistready, and
//! objrepocopy::slavelistdone.
//!
//! The execution thread maintains a weak reference to the destination side
//! object interface that was given to the copycomplete object.
//! If the object goes out of scope
//! and gets destroyed, and the execution thread will abort the next time it
//! tries to send any message to the source side. [DSTGONE] It is expected that
//! when the source side object goes out of scope, an external mechanism will
//! result in stop() getting invoked.
//!
//! If the \c batonresponse reply from the destination side contains a
//! uuid that matches the uuid of the baton passed to the execution thread,
//! the thread marks the copy as succesful and terminates immediately without
//! sending any more messages, releasing its reference to the baton
//! object [BATONSRCCOPYRELEASEDONE]. In all other cases, the execution thread
//! drops its reference on the baton after processing the
//! \c batonresponse message [BATONSRCOPYRELEASEMISMATCH].

class objrepocopysrcthreadObj : public objrepocopysrcinterfaceObj,
				public x::runthreadname,
				virtual public x::threadmsgdispatcherObj {

#include "objrepocopysrcthread.msgs.H"

private:
	//! The logger object
	LOG_CLASS_SCOPE;

	//! This thread's name

	std::string name;

	//! Repository being copied from

	tobjrepo *repo;

	//! The destination side being copied to
	x::weakptr<objrepocopydstinterfaceptr > dst;

	class uuidenumObj;

	class notifierObj;

	//! Enumeration object for the repository

	x::ptr<uuidenumObj> uuidenum;

	//! Retrieve a reference to the source side interface object

	//! An exception gets thrown if the source side interface object
	//! went out of scope.

	objrepocopydstinterface getdst();

	//! Sanity check, only one at a time

	std::mutex dispatch_mutex;

	class dispatch_lock;
public:

	//! Constructor
	objrepocopysrcthreadObj(const std::string &name);

	//! Destructor
	~objrepocopysrcthreadObj() noexcept;

	//! Retrieve this thread's name

	std::string getName() const noexcept;

	//! Repository copy completion notification.

	//! When the execution thread terminates, releasing its
	//! reference on the mcguffin, success() indicates whether or not
	//! the copy operation terminated normally.
	//!
	//! \note
	//! Well, the copy operation is almost complete. The caller is
	//! responsible for invoking release(). Until release() gets
	//! invoked, any changes still being commited to the source repository
	//! will continue to be replicated to the destination repository.
	//!
	//! The external process that initiated the repository copy operation
	//! should stop all further updates to the source repository, then
	//! invoke release(). At that point, the source and the destination
	//! repositories are deemed to have identical contents.
	//!
	//! Invoking release() stops the continued replication and sends the
	//! copycomplete message to the destination repository.
	//!
	//! If the last reference to this object goes out of scope, the
	//! destructor calls release() automatically.
	//!
	//! This object maintains a weak reference on the destination
	//! repository object. If it goes out of scope and gets destroyed,
	//! release() sends nothing.

	class copycompleteObj : virtual public x::obj {

		//! The logger class

		LOG_CLASS_SCOPE;

		//! Whether the copy operation was succesfull
		bool succeededflag;

		//! The mutex protecting the flags
		std::mutex mutex;

		//! The destination side being copied to
		x::weakptr<objrepocopydstinterfaceptr > dst;

		//! Keep the baton around, until this is official
		batonptr batonp;

		//! The notifier object that continues to replicate changes
		x::ptr<notifierObj> notifier;

		//! Set the newly created notifier object
		void setNotifier(const x::ptr<notifierObj> &notifierArg);


		//! Copy operation was succesful
		void setSuccesfull(//! With help from this baton
				   const batonptr &batonArg);


	public:
		friend class objrepocopysrcthreadObj;

		//! Constructor
		copycompleteObj(const objrepocopydstinterfaceptr
				&dstArg);

		~copycompleteObj() noexcept;

		//! Indication that the copy operation was succesfull
		bool success();


		//! Release the copycomplete message and stop replication
		void release();
	};

	//! A reference to an object indicating copy completion status

	typedef x::ref<copycompleteObj> copycomplete;

	//! A nullable pointer reference to copycompleteObj

	typedef x::ptr<copycompleteObj> copycompleteptr;

	//! If the slave produces a matching batonuuid, we're done

	void event(const objrepocopy::batonresponse &msg);


	//! Send the thread a slavelist message

	void event(const objrepocopy::slavelist &msg);


	//! Send the thread a slavelistready message

	void event(const objrepocopy::slavelistready &msg);


	//! Send the thread a slavelistdone message
	void event(const objrepocopy::slavelistdone &msg);

	//! The execution thread

	void run(//! Constructor mcguffin
		 x::ptr<x::obj> &threadmsgdispatcher_mcguffin,

		 //! Start synchronization
		 start_thread_sync &sync_arg,

		 //! Source repository
		 tobjrepo &repocopy,

		 //! Baton, if any
		 batonptr &batonref,

		 //! Completion object
		 copycomplete &ret,

		 //! Thread's mcguffin
		 const x::ptr<x::obj> &mcguffinref);

private:

	//! Pointer to the completion reference, on the execution thread's stack

	copycomplete *complete_ptr;

	//! Pointer to the baton for the source repository

	batonptr *batonp;

	//! Process slavelist message

	void dispatch_handle_slavelist(const objrepocopy::slavelist &msg);

	//! Acquire a commit lock on the repository

	tobjrepoObj::commitlock_t getcommitlock();

	//! Acknowledge slavelist with masterlistdone.

	void dispatch_ack_slavelist_masterlistdone(objuuidlist &uuidList);


	//! Acknowledge slavelist with masterlist.

	void dispatch_ack_slavelist_masterlist(objuuidlist &uuidList);


	//! Acknowledge slavelist with masterack.

	void dispatch_ack_slavelist_masterack(objuuidlist &uuidList);

};

//! A pointer to an objrepocopysrcthreadObj object.

typedef x::ptr<objrepocopysrcthreadObj> objrepocopysrcthreadptr;

#endif

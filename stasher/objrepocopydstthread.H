/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objrepocopydstthread_H
#define objrepocopydstthread_H

#undef libx_autogen_suppress_inner

#include "objrepocopydstinterface.H"
#include "objrepocopydstthreadfwd.H"
#include "objrepocopysrcinterfacefwd.H"
#include "boolref.H"
#include "tobjrepofwd.H"
#include "batonfwd.H"
#include <x/threads/runthreadname.H>
#include <x/eventqueuemsgdispatcher.H>
#include <x/weakptr.H>
#include <x/logger.H>

//! Execution thread implementation of the destination side of a repo copy

//! run() executes an execution thread for the destination side of
//! a repository copy operation. run() takes the following parameters:
//!
//! - a reference to the destination repository
//!
//! - a reference to the source side's interface, for responding
//! to the source (master) side.
//!
//! - a boolref
//!
//! - a baton, if one exists for the destination repo.
//!
//! - a generic mcguffin object.
//!
//! The execution thread keeps a weak reference to the source side object.
//! Invoke event() to process messages received from the master. The messages
//! are: objrepocopy::masterlist, objrepocopy::masterlistdone,
//! objrepocopy::slaveliststart, objrepocopy::masterack, and
//! objrepocopy::copycomplete.
//!
//! objrepocopy::copycomplete results in the thread stopping. [RECVCOPYDONE]
//! Additionally, stop() also results in the thread stopping at the
//! next possible opportunity. [RECVSTOPREQUEST]
//!
//! The execution thread maintains a weak reference to the source side object
//! that was passed to start(). If the source side object goes out of scope
//! and gets destroyed, the execution thread will abort the next time it
//! tries to send any message to the source side. [SRCGONE] It is expected that
//! when the source side object goes out of scope, an external mechanism will
//! result in stop() getting invoked.
//!
//! This object maintains a reference to the mcguffin object while it is
//! active. It releases its reference to the mcguffin when the thread
//! terminates. If it receives a copycomplete message, the boolref's flag
//! gets set to true before the thread terminates. [MCGUFFIN]
//!
//! In response to a \c batonrequest message, the thread sends a
//! \c batonresponse containing the uuid of the baton it received in its
//! starting argument (the uuid string is blank if a null baton reference was
//! specified as the starting argument).
//!
//! The thread releases its reference on the baton when it naturally terminates
//! upon receipt of a \c copycomplete message [BATONDSTCOPYRELEASECOMPLETE],
//! or when it receives a \c masterlist message (indicating that the source side
//! did not have a matching baton) [BATONDSTCOPYRELEASESLAVELIST].

class objrepocopydstthreadObj : public objrepocopydstinterfaceObj,
				public x::runthreadname,
				virtual public x::eventqueuemsgdispatcherObj {

#include "objrepocopydstthread.msgs.all.H"

private:
	//! The logger object
	LOG_CLASS_SCOPE;

	//! Repository being copied to

	tobjrepo *repo;

	//! The source side being copied from
	x::weakptr<objrepocopysrcinterfaceptr> src;

	//! Pointer to the boolref, on the executing thread's stack
	boolrefObj *flag;

	//! Pointer to the repository's baton, if given, on the stack
	batonptr *batonp;

	class uuidenumObj;

	//! Enumeration object for the repository

	x::ptr<uuidenumObj> uuidenumref;

	//! Retrieve a reference to the source side interface object

	//! An exception gets thrown if the source side interface object
	//! went out of scope.

	objrepocopysrcinterface getsrc();

	//! Sanity check, only one at a time

	std::mutex dispatch_mutex;

	class dispatch_lock;

	//! Name of this thread
	std::string name;

public:
	//! Constructor
	objrepocopydstthreadObj(const std::string &name);

	//! Destructor
	~objrepocopydstthreadObj() noexcept;

	//! Return this thread's name
	std::string getName() const noexcept;

	//! Send the thread a batonrequest message
	void event(const objrepocopy::batonrequest &msg);


	//! Send the thread a masterlist message
	void event(const objrepocopy::masterlist &msg);


	//! Send the thread a masterlistdone message

	void event(const objrepocopy::masterlistdone &msg);


	//! Send the thread a slaveliststart message
	void event(const objrepocopy::slaveliststart &msg);


	//! Send the thread a masterack message
	void event(const objrepocopy::masterack &msg);


	//! Send the thread a copycomplete message
	void event(const objrepocopy::copycomplete &msg);


	//! Send the thread an objserializer message
	void event(const objserializer &msg);


private:

	//! Create a response to an objrepocopy::masterlist message from master

	void process_dispatch(const objrepocopy::masterlist &msg,
			      objrepocopy::slavelist &ack);

public:
	//! The execution thread

	void run(tobjrepo &repocpy,
		 x::weakptr<objrepocopysrcinterfaceptr> &srcArg,
		 boolref &flagref,
		 batonptr &batonref,
		 const x::ptr<x::obj> &mcguffin);
};

#endif

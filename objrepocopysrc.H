/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objrepocopysrc_H
#define objrepocopysrc_H

#include "objrepocopysrcthread.H"
#include "threadmgr.H"
#include "objrepocopysrcfwd.H"

//! Repository copy - source side

//! A facade for objrepocopysrcthreadObj. Since an execution thread holds a
//! reference to its own object, if the last external reference to
//! objrepocopysrcthreadObj goes out of scope, the execution thread continues
//! to run, and won't get destroyed. This facade's destructor invokes
//! stop(), and wait()s until the execution thread stops, if it's
//! still running when the last reference to this object goes out of scope and
//! the destructor gets invoked.
//!
//! Invoke start() to begin a repository copy to a destination repository.
//! start() returns a copycomplete reference. Use completed() to check if
//! the repository copy operation completed, otherwise wait on the event
//! file descriptor, and try again. success() indicats whether the primary
//! copy operation completed succesfully (or if the thread died, for some
//! reason).
//!
//! Stop all commits to the source repository, then invoke release(). When
//! release() returns, the destination repository may be considered to have
//! the same contents as the source repository.
//!
//! Invoke event() to process messages received from the destination
//! repository copy thread.

class objrepocopysrcObj
	: public threadmgrObj<objrepocopysrcthreadptr>,
	  public objrepocopysrcinterfaceObj {

	typedef threadmgrObj<objrepocopysrcthreadptr> thread_owner_t;

public:

	//! Reference returned by start()

	typedef objrepocopysrcthreadObj::copycomplete copycomplete;

	//! A nullable reference pointer returned by start()
	typedef objrepocopysrcthreadObj::copycompleteptr copycompleteptr;

	//! Constructor
	objrepocopysrcObj();

	//! Destructor
	~objrepocopysrcObj();

	//! Begin copy operation
	copycomplete start(const tobjrepo &repoArg,
			   const objrepocopydstinterfaceptr &dstArg,
			   const batonptr &batonArg,
			   const x::ptr<x::obj> &mcguffin);


	//! Received a batonresponse message
	void event(const objrepocopy::batonresponse &msg) override;

	//! Received a slavelist message
	void event(const objrepocopy::slavelist &msg) override;

	//! Received a slavelistready message
	void event(const objrepocopy::slavelistready &msg) override;

	//! Received a slavelistdone message
	void event(const objrepocopy::slavelistdone &msg) override;
};

#endif

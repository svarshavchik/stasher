/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objrepocopydst_H
#define objrepocopydst_H

#include "objrepocopydstthread.H"
#include "objrepocopydstfwd.H"
#include "threadmgr.H"
#include "batonfwd.H"

//! Repository copy - destination side

//! A facade for objrepocopydstthreadObj. Since an execution thread holds a
//! reference to its own object, if the last external reference to
//! objrepocopydstthreadObj goes out of scope, the execution thread continues
//! to run, and won't get destroyed. This facade's destructor invokes
//! stop(), and wait()s until the execution thread stops, if it's
//! still running when the last reference to this object goes out of scope and
//! the destructor gets invoked.
//!
//! Invoke start() to begin a repository copy from a source repository.
//!
//! Invoke event() to process messages received from the source repository.
//! After receiving the copycomplete message, the execution thread terminates.
//!
//! \note
//! objserializer messages effect the actual copy, and need to be deserialized
//! and processed manually.

class objrepocopydstObj
	: public threadmgrObj<objrepocopydstthreadptr>,
	  public objrepocopydstinterfaceObj {

	//! Superclass

	typedef threadmgrObj<objrepocopydstthreadptr> thread_owner_t;

public:
	objrepocopydstObj();
	~objrepocopydstObj();

	//! Start the execution thread

	void start(const tobjrepo &repoArg,
		   const objrepocopysrcinterfaceptr &srcArg,
		   const boolref &flagArg,
		   const batonptr &batonArg,
		   const x::ptr<x::obj> &mcguffinArg);


	//! Received a batonrequest message
	void event(const objrepocopy::batonrequest &msg) override;

	//! Received a masterlist message
	void event(const objrepocopy::masterlist &msg) override;

	//! Received a masterlistdone message
	void event(const objrepocopy::masterlistdone &msg) override;

	//! Received a slaveliststart message
	void event(const objrepocopy::slaveliststart &msg) override;

	//! Received a masterack message
	void event(const objrepocopy::masterack &msg) override;

	//! Received a copycomplete message
	void event(const objrepocopy::copycomplete &msg) override;

private:
	//! Not implemented

	void event(const objserializer &msg) override;
};

#endif

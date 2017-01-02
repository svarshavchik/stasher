/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objwriterthread_H
#define objwriterthread_H

#include "objsink.H"
#include "objwriter.H"
#include "writtenobjbase.H"
#include <x/eventfd.H>
#include <x/eventqueue.H>
#include <x/threadmsgdispatcher.H>
#include <x/logger.H>
#include <x/property_value.H>

#include <list>
#include <queue>

#include "threadreportimpl.H"

STASHER_NAMESPACE_START

//! A separate thread for writing objects

//! This is a subclass of objwriterObj that implements a thread that writes
//! objects. objwriterthreadObj does not implement flush(), that
//! still must be implemented in a subclass.
//!
//! Note that because a running thread holds a reference on itself,
//! if all external reference to this object go out of scope, this thread
//! object continues to run! As such, this class should not be used
//! directly, but via fdobjwriterthreadObj.
//!
//! \par Queueing up objects [WRITE]
//!
//! write() queues up an object to be written. write() returns immediately,
//! and the queued objects get written by the thread in the order they were
//! queued.
//!
//! \par Stopping the writer thread, nicely [CLOSE]
//!
//! request_close() returns immediately. When the thread finishes writing
//! any objects that have been queued up before request_close(), the writing
//! thread will terminate.
//!
//! \par Subclass implementation [SUBCLASS]
//!
//! The subclass must implement flush(), that's inherited from objwriterObj.
//!
//! If the subclass needs to wait before it can implement a flush, the
//! subclass must obtain msgqueue->getEventfd() and also wait for the
//! file descriptor to become readable.
//!
//! After the subclass finishes waiting, whether it's because it's ready
//! to write, or if the event file descriptor became readable, the subclass
//! must invoke process_messages().
//!
//! setmaxqueuesize() sets the optional maximum queue size. If the pending
//! number of objects in the queue exceeds the given queue size, the writer
//! thread dies [MAXWRITEQUEUESIZE].

class objwriterthreadObj : protected objwriterObj,
			   public objsinkObj,
			   public threadreportimplObj,
			   virtual public x::threadmsgdispatcherObj
{
public:
	void write(const x::ref<writtenobjbaseObj> &object) override;

#include "objwriterthread.msgs.H"

private:
	//! This thread's name

	std::string name;

public:
	using objwriterObj::default_bufsize;

	//! Return this thread's name

	std::string getName() const noexcept;
private:
	//! The logger

	LOG_CLASS_SCOPE LIBCXX_HIDDEN;

	//! Maximum number of unwritten messages, before the thread dies.

	//! If 0, there is no built-in limit.
	//!
	size_t max_request_count;

	//! Warning level to report for the size of the request queue

	size_t request_count_warn_level;

private:
	//! The timeout property

	x::property::value<unsigned> *timeout;

	class requestObj;

	//! A reference to the object to be written by the thread

	typedef x::ref<requestObj> request;

	class writeRequestObj;
	class stopRequestObj;

	//! A list of pending requests

	typedef std::queue<request> requestlist_t;

	//! Pending requests

	requestlist_t requests;

	class closeexception;

protected:

	//! The running thread
	void run(msgqueue_auto &msgqueue,
		 const x::ref<x::obj> &);

	//! Process messages for this thread
	void process_messages();
public:

	//! Constructor

	objwriterthreadObj(const std::string &nameArg,
			   size_t bufsize=default_bufsize.getValue());


	//! Destructor
	~objwriterthreadObj();

	friend class flushrequestObj;

protected:
	//! Flush anything that's currently buffered

	void writeflush();
private:

	//! Report this thread's status
	std::string report(std::ostream &rep);

};

STASHER_NAMESPACE_END

#endif

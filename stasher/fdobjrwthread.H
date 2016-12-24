/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef fdobjrwthread_H
#define fdobjrwthread_H

#include "fdobjreaderthread.H"
#include "fdobjwriterthread.H"
#include "stoppablethreadtracker.H"

#include <x/logger.H>

STASHER_NAMESPACE_START

//! Base class for fdobjrwthreadObj

//! This base class provides the definitions of
//! \ref startarg "the startup argument" that's created by
//! create_startarg(). Also several internal classes.

class fdobjrwthreadBase {

public:
	//! Property -- the timeout for writing to the file descriptor.
	static x::property::value<unsigned> writeTimeout LIBCXX_HIDDEN;

	//! Property -- the timeout for readinf grom the file descriptor.
	static x::property::value<unsigned> readTimeout LIBCXX_HIDDEN;
};

//! A thread that read and writes objects from a file descriptor, and handles messages

//! This class subclasses fdobjreaderthreadObj.
//! The subclass must implement
//! the classlist() and overloaded deserialized() methods that receive
//! deserialized objects from the file descriptor, as specified in the
//! deserialization API. [DESERIALIZATION]
//!
//! The subclass also implements overloaded dispatch()
//! methods that get invoked when the thread receives a message. [DISPATCH]
//! The subclass also inherits the overloaded event() method that sends those
//! messages to the thread. Messages sent to the thread via event() result
//! in the corresponding dispatch() method being invoked in the context of the
//! thread.
//!
//! \par The main loop
//!
//! The subclass is expected to invoke mainloop() passing it the following
//! parameters:
//!
//! - a NONBLOCKING file descriptor transport for reading and writing objects.
//!
//! - an already existing input iterator that's connected to the transport,
//! for reading.
//!
//! - a stoppableThreadTracker
//!
//! - a generic mcguffin object reference.
//!
//! mainloop() starts a separate thread that writes messages to the
//! file descriptor transport. The thread gets started using the
//! stoppableThreadTracker [TRACKER]. The native
//! \ref fdobjrwthreadObj::writer "writer" pointer points to the started
//! thread. This native
//! pointer is valid while mainloop() is running.
//! mainloop() enters a loop that reads and deserialized messages from the
//! file descriptor, and dispatches received messages, in the context of
//! a single execution thread, and the writing thread runs separately.
//!
//! mainloop() returns when the file descriptor transport is closed. [CLOSED]
//! If the independent writing thread terminates for any reason, a thrown
//! exception also terminates mainloop(). [WRITERABORTS]
//!
//! To return the favor, if mainloop() terminates for any reason, the
//! writer thread also terminates. [MAINABORTS]
//! Invoking stop() for this thread
//! terminates both the mainloop() execution thread (with an exception) and
//! the writing thread. [RWSTOP]
//!
//! The subclass may override started() to perform initialization tasks right
//! after mainloop() finishes initializing the
//! \ref fdobjrwthreadObj::writer "writer" pointer.
//!
//! In all cases, the native \ref fdobjrwthreadObj::writer "writer" pointer
//! remains valid until
//! mainloop() terminates. In addition to the writer thread separately being
//! registered with a stoppableThreadTracker, the writing thread
//! also gets stopped when mainloop() terminates, in all cases. The actual
//! execution thread might terminate, and the writing thread object getting
//! destroyed, either before or after mainloop() returns.
//!
//! \par The mcguffin [RWMCGUFFIN]
//!
//! The mcguffin object reference is held by both the main reader
//! thread and the writer thread. If there are no other references to the
//! mcguffin, it gets destroyed only after both threads have stopped, in
//! whatever order they ended up getting stopped. The objects may still be
//! in the process of being destroyed, but the execution threads are finished
//! with.
//!
//! A destructor callback on the mcguffin gets invoked after both threads
//! are done.

class fdobjrwthreadObj: public fdobjreaderthreadObj,
			public fdobjrwthreadBase {

	//! The logger

	LOG_CLASS_SCOPE LIBCXX_HIDDEN;

	//! The writer thread's name, as passed to the constructor
	std::string writeThreadName;

protected:
	//! Read timeout, initialized by the constructor. May be overridden

	unsigned readTimeout_value;

public:
	//! Constructor
	fdobjrwthreadObj(const std::string &writeThreadNameArg);


	//! Destructor
	~fdobjrwthreadObj() noexcept;

	//! Deserialization classlist enumerator

	template<typename iter_type>
	static void classlist(iter_type &iter)
	{
		iter.template serialize<fdobjwriterthreadObj::sendfd_coming>();
	}

	//! Deserializing a file descriptor
	void deserialized(const fdobjwriterthreadObj::sendfd_coming &msg);


private:
	//! Invoked to process a deserialized file descriptor

	virtual void received_fd(const std::vector<x::fdptr> &fdArg);


	//! Before receiving file descriptors, object now or hold your peace

	virtual void accept_fds(//! How many file descriptors are coming
				size_t n);

	//! A poll array for the transport file descriptor and the message queue event file descriptor

	//! \internal
	//!
	struct pollfd pfd[2];

protected:

	//! Wait for the file descriptor to become readable  (or for an event to become available).

	//! Calls drain(), then polls the socket and the event file descriptor.

	void wait_socket(int timeout);

	//! Wait only for the event queue to become readable

	//! Calls drain(), then polls the event file descriptor

	void wait_eventqueue(int time);

	//! The running writer thread

	//! This native pointer is valid as long as mainloop() is running.
	//!

	fdobjwriterthreadObj *writer;

	//! Run the main loop

	//! The subclass must invoke mainloop() in the context of the
	//! execution thread.

	void mainloop(//! Thread message queue
		      msgqueue_auto &msgqueue,

		      //! The file descriptor
		      const x::fdbase &transport,

		      //! The input iterator for the file descriptor
		      const x::fd::base::inputiter &inputiter,

		      //! Thread tracker that the writer thread gets registered with
		      const stoppableThreadTracker &tracker,

		      //! The fdobjrwthreadObj mcguffin
		      const x::ptr<x::obj> &mcguffin);

private:

	//! Callback: the thread has started

	//! May be overriden by subclass to perform some startup stuff
	virtual void started();

	//! Read the file descriptor transport for more messages.

	//! \internal

	size_t pubread(const x::fdbase &transport,
		       char *buffer,
		       size_t cnt);
};

STASHER_NAMESPACE_END

#endif

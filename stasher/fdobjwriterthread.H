/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef fdobjwriterthread_H
#define fdobjwriterthread_H

#undef libx_autogen_suppress_inner

#include "objwriterthread.H"
#include <x/fd.H>
#include <x/uuid.H>

STASHER_NAMESPACE_START

//! Write objects to a file descriptor.

//! Implement objwriterthreadObj on top of a file descriptor.
//!
//! \par Starting the writer thread
//!
//! The constructor receives the thread's name, a pointer to a property, which
//! may be a NULL pointer, that defines the write timeout, and the size of
//! the buffer.
//!
//! start(), inherited from the thread object, takes a file descriptor
//! transport reference as a paramter.
//!
//! The property, if not a NULL pointer, sets the timeout for writing. If
//! the non-blocking file descriptor is not writable for the duration of time
//! specified by this property, the thread terminates. [WRITESTART]
//!
//! If start() is called again while the writer thread is running,
//! an exception gets thrown. But if the writer thread has terminated,
//! start() may be called again. [RECYCLE]
//!
//! \par Sending file descriptors over a unix domain socket
//!
//! If the underlying file descriptor is a unix domain socket, sendfd()
//! queues up a message that sends file descriptors over a unix domain socket.
//!
//! What this does is send a sendfd_coming message, flush the output buffer
//! and wait holding all writes, until the peer presumably responds with a
//! sendfd_ready message. The reader side is expected to invoke the
//! sendfd_proceed() message, with then sends the file descriptors over the now
//! empty socket [SENDFD].
//!
//! \par Write error
//!
//! An error writing to a socket results in a thrown exception and termination
//! of the writing thread [ERROR]. This includes a timeout. [WRITETIMEOUT]

class fdobjwriterthreadObj : public objwriterthreadObj {

	//! Logger

	LOG_CLASS_SCOPE LIBCXX_HIDDEN;

	//! One thread at a time, please.
	std::mutex thread_lock;

	//! Transport on the execution task's stack
	x::fdbaseObj *transport;

	//! Timeout property
	x::property::value<unsigned> *timeout;

	//! Precreated poll(2) array

	struct pollfd pfd[2];

	//! Inherited from objwriterthreadObj

	//! \internal

	size_t flush(const char *ptr, size_t cnt) LIBCXX_HIDDEN;

	//! A mcguffin used to send a file descriptor

	//! Sending a file descriptor involves installing this mcguffin and
	//! sending a \c sendfd_coming message, waiting for this mcguffin to get
	//! destroyed, then sending the file descriptor over the socket.
	//!
	//! Upon receipt of \c sendfd_coming, the peer replies with a 
	//! \c sendfd_ready. Upon receipt of sendfd_ready, sendfd_proceed()
	//! gets invoked, which destroys the mcguffin.
	//!
	//! This a pointer to the mcguffin object on the executing thread's
	//! stack.

	x::ptr<x::obj> *sendfd_mcguffin;

public:
	//! Message used in sending file descriptors over sockets

	class sendfd_ready {
	public:
		//! Default constructor
		sendfd_ready() {}

		//! Destructor
		~sendfd_ready() noexcept {}

		//! Convenience constructor for deserialization

		template<typename parent_type>
		sendfd_ready(parent_type &dummy) {}

		//! Serialization function

		template<typename iter_type>
		void serialize(iter_type &iter)
		{
		}
	};

	//! Message used in sending file descriptors over sockets

	class sendfd_coming {
	public:
		//! How many file descriptors
		size_t count;

		//! Constructor
		sendfd_coming(size_t n) : count(n) {}

		//! Default constructor
		sendfd_coming() {}

		//! Convenience constructor for deserializing
		template<typename impl_type>
		sendfd_coming(impl_type &dummy) {}

		//! Destructor
		~sendfd_coming() noexcept {}

		//! Serialization function

		template<typename iter_type>
		void serialize(iter_type &iter)
		{
			iter(count);
		}
	};

	//! Constructor
	fdobjwriterthreadObj(//! Name of this thread, for logging purposes
			     const std::string &threadName,

			     //! Write timeout property to use
			     x::property::value<unsigned> *timeout=NULL,

			     //! Output buffer size
			     size_t bufsize=default_bufsize.getValue());

	~fdobjwriterthreadObj() noexcept;

	//! The execution thread

	//! The file descriptor gets passed as the starting argument.
	//! Initialize transport, pollfd, then invoke objwriterthreadObj::run().

	void run(const x::fdbase &fd, const x::ref<x::obj> &mcguffin);

	//! Send a file descriptor

	void sendfd(const std::vector<x::fd> &filedescs);

	//! We don't have file descriptors now, but will have them, promise.

	//! If we don't have file descriptors to send right now, but we promise
	//! to get them when the thread is actually ready to push it out, pass
	//! a reference to this class, and implement getfd() in the subclass

	class getsendfd : virtual public x::obj {

	public:
		getsendfd();
		~getsendfd() noexcept;

		//! Return file descriptors to send, or else

		//! A thrown exception kills the writer thread. A failure to
		//! return an open file descriptor throws an exception.
		virtual const std::vector<x::fd> &getFd()=0;

		class default_impl;
	};

private:
	class sendfdreqObj;

	//! Send the file descriptor, invoked in the context of the executing thread

	//! \internal
	void dosendfd(const std::vector<x::fd> &fd)
		LIBCXX_HIDDEN;
public:
	friend class sendfdreqObj;

#include "fdobjwriterthread.msgs.decl.H"

};

STASHER_NAMESPACE_END

#endif

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef fdobjreaderthread_H
#define fdobjreaderthread_H

#include <x/eventqueuemsgdispatcher.H>
#include <x/fditer.H>
#include <string>
#include "objsource.H"

STASHER_NAMESPACE_START

//! A thread that reads objects from a file descriptor and handles messages.

//! This template class combines message dispatching with object deserialization
//! from a file descriptor.
//!
//! event(), inherited from x::eventqueuemsgdispatcherObj, sends messages to the
//! thread [MESSAGE].
//!
//! The subclass implements dispatch() methods to receive messages that were
//! sent to it.
//!
//! The subclass also implements classlist() and deserialized() methods,
//! as defined by the deserialization API, to handle messages deserialized
//! from the file descriptor.
//!
//! The subclass typically has a run() method that gets invoked by a new
//! thread, which then invokes mainloop().
//!
//! The subclass passes to mainloop() an x::fd::base::inputiter,
//! the file descriptor transport to which it belongs (it should know that),
//! and a native reference to itself.
//!
//! mainloop() returns when the file descriptor iterator becomes the ending
//! iterator. [EOF]
//!
//! mainloop() returns when the thread is stop()ed. {STOP]
//!
//! pubread() gets invoked to read from the file descriptor. The subclass
//! must implement pubread() to wait until either the file descriptor is
//! readable, or msgqueue->getEventfd() is readable. Before waiting,
//! the subclass must invoke drain(). If the subclass wakes up because
//! of msgqueue->getEventfd() and the file descriptor is not readable,
//! drain() must be invoked again before the subclass continues waiting.
//! [DRAIN]
//!
//! pubread() gets invoked only when the underlying transport returns 0 for
//! pubread_pending(), otherwise the transport's pubread() gets invoked to
//! read the pending data without invoking subclass's pubread().
//!
//! \note
//! The underlying file descriptor should be non-blocking, and pubread()
//! should try again for EAGAIN or EWOULDBLOCK (invoking drain() before
//! it waits again), and return 0 only when the socket is closed.
//!
//! \note
//! A subclass may tack on additional processing that triggers the event
//! file descriptor by overriding drain(). The subclass must invoke
//! drain() from the superclass first, because this drains the event file
//! descriptor, then check for any pending processing that triggers the
//! event file descriptor.

class fdobjreaderthreadObj : virtual public x::eventqueuemsgdispatcherObj {

public:
	//! Constructor
	fdobjreaderthreadObj();

	//! Destructor
	~fdobjreaderthreadObj() noexcept;

private:

	//! Subclass must define it, as per contract.

	virtual size_t pubread(//! Original transport received by mainloop()
			       const x::fdbase &transport,
			       //! Buffer to read into
			       char *buffer,
			       //! Buffer's size
			       size_t cnt)=0;

	class fdadapterObj;
public:
	friend class fdadapterObj;


protected:

	//! The socket, on the executing thread's stack
	x::fdptr *socket;

	//! The subclass must call drain() before waiting.

	virtual void drain();

	//! Implement the subclass's main loop.

	//! mainloop() deserializes objects and dispatches messages until
	//! an exception gets thrown or the file descriptor goes into EOF
	//! state.
	void mainloop(//! Transport being read from
		      const x::fdbase &transport,

		      //! Input iterator that's connected to the transport
		      x::fd::base::inputiter &beg_iter);

	//! The subclass must implement mainloop_impl

	//! The subclass should simply stick MAINLOOP_IMPL into the class
	//! definition, to fullfill the requirement.

	virtual void mainloop_impl(x::fd::base::inputiter &b,
				   x::fd::base::inputiter &e)
=0;

	//! A convenience macro to declare mainloop_impl in a subclass
#define MAINLOOP_DECL							\
	void mainloop_impl(x::fd::base::inputiter &b, x::fd::base::inputiter &e) \


	//! A convenience macro to implement some class's mainloop_impl

#define MAINLOOP_IMPL(classname)					\
	void classname::mainloop_impl(x::fd::base::inputiter &b,	\
				      x::fd::base::inputiter &e)	\
					\
	{								\
		STASHER_NAMESPACE::objsource(b, e, *this);		\
	}
};

STASHER_NAMESPACE_END

#endif

/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "fdobjwriterthread.H"
#include "objwriterthread.H"
#include "writtenobjbase.H"
#include <x/property_value.H>
#include <x/eventdestroynotify.H>
#include <x/sysexception.H>

LOG_CLASS_INIT(STASHER_NAMESPACE::fdobjwriterthreadObj);

STASHER_NAMESPACE_START

fdobjwriterthreadObj::fdobjwriterthreadObj(const std::string &threadName,
					   x::property::value<unsigned>
					   *timeoutArg,
					   size_t bufsize)
	: objwriterthreadObj(threadName, bufsize),
	  timeout(timeoutArg)
{
}

fdobjwriterthreadObj::~fdobjwriterthreadObj()
{
}

size_t fdobjwriterthreadObj::flush(const char *ptr, size_t cnt)

{
	size_t n;
	time_t now=time(NULL), expired=now, warning=now;

	if (timeout)
	{
		expired += timeout->getValue();
		warning += (expired-now)/2;
	}

	while (1)
	{
		n=transport->pubwrite(ptr, cnt);

		if (n > 0)
			return n;

		if (errno != EAGAIN && errno != EWOULDBLOCK)
			throw SYSEXCEPTION("write");

		if (!timeout)
			expired=now + 300;

		if (now >= expired)
			throw EXCEPTION(getName() + ": timed out");

		process_messages();

		int n=poll(pfd, 2, (expired-now)*1000);

		if (n < 0)
		{
			if (errno != EINTR)
				throw SYSEXCEPTION("poll");
		}

		now=time(NULL);

		if (n == 0)
			continue; // Should timeout on the next pass

		if (pfd[0].revents & (POLLERR|POLLHUP))
			throw EXCEPTION("poll: socket closed or an error condition");

		if ((pfd[0].revents & POLLOUT) && timeout && now >= warning)
		{
			LOG_WARNING(getName() + ": experiencing delays");
		}
	}
}

void fdobjwriterthreadObj::run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
			       const x::fdbase &fd,
			       const x::ref<x::obj> &mcguffin)
{
	msgqueue_auto msgqueue(this);
	threadmsgdispatcher_mcguffin=nullptr;

	transport= &*fd;

	pfd[0].fd=transport->getFd();
	pfd[0].events=POLLOUT;

	pfd[1].fd=msgqueue->getEventfd()->getFd();
	pfd[1].events=POLLIN;

	x::ptr<x::obj> sendfd_mcguffinref;

	sendfd_mcguffin=&sendfd_mcguffinref;

	objwriterthreadObj::run(msgqueue, fd);
}

class fdobjwriterthreadObj::sendfdreqObj : public writtenobjbaseObj {

public:
	fdobjwriterthreadObj &fdwriter;

	x::ref<getsendfd> gethandler;

	sendfdreqObj(fdobjwriterthreadObj &fdwriterArg,
		     const x::ref<getsendfd> &gethandlerArg)
		: fdwriter(fdwriterArg), gethandler(gethandlerArg)
	{
	}

	~sendfdreqObj()
	{
	}

	void serialize(class objwriterObj &writer)
	{
		fdwriter.dosendfd(gethandler->getFd());
	}
};

fdobjwriterthreadObj::getsendfd::getsendfd()
{
}

fdobjwriterthreadObj::getsendfd::~getsendfd()
{
}

// The simple version of sendfd() just uses this.

class LIBCXX_INTERNAL fdobjwriterthreadObj::getsendfd::default_impl
	: public getsendfd {

 public:
	std::vector<x::fd> fd;

	default_impl(const std::vector<x::fd> &fdArg)
 : fd(fdArg) {}
	~default_impl() {}

	const std::vector<x::fd> &getFd() { return fd; }
};


void fdobjwriterthreadObj::sendfd(const std::vector<x::fd> &filedescs)

{
	write(x::ref<sendfdreqObj>::create(*this,
					   x::ref<getsendfd::default_impl>
					   ::create(filedescs)));
}

void fdobjwriterthreadObj::dosendfd(const std::vector<x::fd> &filedesc)

{
	{
		if (filedesc.empty())
			throw EXCEPTION("Failed to retrieve file descriptors for sending");
	}

	x::weakptr<x::ptr<x::obj> > wmcguffin;

	{
		wmcguffin=*sendfd_mcguffin=x::ptr<x::obj>::create();

		sendfd_coming comingmsg(filedesc.size());

		serialize(comingmsg);

		x::eventdestroynotify::create(get_msgqueue()->getEventfd(),
					      *sendfd_mcguffin);
	}

	writeflush();

	LOG_TRACE("Waiting to send " << filedesc.size() << " file descriptors");

	while (!wmcguffin.getptr().null())
	{
		process_messages();

		int n=poll(pfd+1, 1, -1);

		if (n < 0)
		{
			if (errno != EINTR)
				throw SYSEXCEPTION("poll");
		}
	}

	LOG_TRACE("Sending file descriptors");

	while (!dynamic_cast<x::fdObj &>(*transport).send_fd(filedesc))
	{
		process_messages();

		int n=poll(pfd, 2, -1);

		if (n < 0)
		{
			if (errno != EINTR)
				throw SYSEXCEPTION("poll");
		}
	}
}

void fdobjwriterthreadObj::dispatch_sendfd_proceed(const sendfd_ready &ack)

{
	LOG_TRACE("Peer is ready for a file descriptor");
	*sendfd_mcguffin=nullptr;
}

STASHER_NAMESPACE_END

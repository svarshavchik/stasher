/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "fdobjrwthread.H"
#include "writtenobj.H"

#include <x/sysexception.H>

LOG_CLASS_INIT(STASHER_NAMESPACE::fdobjrwthreadObj);

STASHER_NAMESPACE_START

x::property::value<unsigned> fdobjrwthreadBase::writeTimeout("objrepo::timeoutwrite",
							     10);
x::property::value<unsigned> fdobjrwthreadBase::readTimeout("objrepo::timeoutread",
							     90);

fdobjrwthreadObj::fdobjrwthreadObj(const std::string &writeThreadNameArg)
	: writeThreadName(writeThreadNameArg),
	  readTimeout_value(readTimeout.getValue())
{
}

fdobjrwthreadObj::~fdobjrwthreadObj() noexcept
{
}

void fdobjrwthreadObj::mainloop(const x::fdbase &transport,
				const x::fd::base::inputiter &inputiterArg,
				const stoppableThreadTracker &tracker,
				const x::ptr<x::obj> &mcguffin)
{
	x::fd::base::inputiter inputiter(inputiterArg);

	auto w=x::ref<fdobjwriterthreadObj>::create(writeThreadName,
						    &writeTimeout,
						    fdobjwriterthreadObj
						    ::default_bufsize
						    .getValue());

	x::ref<x::obj> my_mcguffin=x::ref<x::obj>::create();

	{
		x::ref<x::obj> writer_mcguffin=x::ref<x::obj>::create();

		x::stoppable::base::group stoppable_group=
			x::stoppable::base::group::create();

		stoppable_group->add(w);
		stoppable_group->add(x::ref<fdobjrwthreadObj>(this));

		stoppable_group->mcguffin(my_mcguffin);
		stoppable_group->mcguffin(writer_mcguffin);

		tracker->start(w, transport, writer_mcguffin);
	}


	writer= &*w;

	pfd[0].fd=transport->getFd();
	pfd[0].events=POLLIN;

	pfd[1].fd=this->msgqueue->getEventfd()->getFd();
	pfd[1].events=POLLIN;

	started();
	fdobjreaderthreadObj::mainloop(transport, inputiter);
	LOG_TRACE("Terminated");
}

void fdobjrwthreadObj::started()
{
}

size_t fdobjrwthreadObj::pubread(const x::fdbase &transport,
				 char *buffer,
				 size_t cnt)
{
	size_t n;
	time_t now=time(NULL), expired=now + readTimeout_value;

	if (readTimeout_value == 0)
		expired=now+300; // No effective timeout

	while ((n=transport->pubread(buffer, cnt)) == 0)
	{
		if (errno == 0)
			break;

		if (errno != EAGAIN && errno != EWOULDBLOCK)
			throw SYSEXCEPTION("read");

		wait_socket((expired - now) * 1000);

		now=time(NULL);

		if (readTimeout_value == 0)
			expired=now+300; // No effective timeout

		if (now >= expired)
		{
			errno=ETIMEDOUT;
			throw SYSEXCEPTION("read");
		}
	}

	return n;
}

void fdobjrwthreadObj::wait_socket(int timeout)
{
	drain();

	int rc=::poll(pfd, 2, timeout);

	if (rc < 0)
	{
		if (errno != EINTR)
			throw SYSEXCEPTION("poll");
	}
}

void fdobjrwthreadObj::wait_eventqueue(int timeout)
{
	drain();

	int rc=::poll(pfd+1, 1, timeout);

	if (rc < 0)
	{
		if (errno != EINTR)
			throw SYSEXCEPTION("poll");
	}
}

void fdobjrwthreadObj::deserialized(const fdobjwriterthreadObj::sendfd_coming
				    &msg)

{
	writer->write(x::ptr<writtenObj<fdobjwriterthreadObj::sendfd_ready> >
		      ::create());

	do
	{
		wait_socket(-1);
	} while (pfd[0].revents == 0);

	std::vector<x::fdptr> fd;

	fd.resize(msg.count);

	(*socket)->recv_fd(fd);

	if (fd.size() != msg.count)
	{
		if (fd.size() == 0)
		{
			LOG_DEBUG("Did not receive expected file"
				  " descriptors, most likely a connection shutdown");
			throw x::stopexception();
		}
		throw EXCEPTION("Failed to receive expected file descriptors");
	}

	received_fd(fd);
}

void fdobjrwthreadObj::received_fd(const std::vector<x::fdptr> &fdArg)

{
}

void fdobjrwthreadObj::accept_fds(size_t n)
{
	throw EXCEPTION("Unexpected file descriptors");
}

STASHER_NAMESPACE_END

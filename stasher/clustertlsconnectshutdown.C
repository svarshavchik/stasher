/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "clustertlsconnectshutdown.H"
#include "stoppablethreadtracker.H"

#include <x/timespec.H>

LOG_CLASS_INIT(clustertlsconnectshutdownObj);

x::property::value<unsigned>
clustertlsconnectshutdownObj::bye_timeout("byetimeout", 15);

unsigned clustertlsconnectshutdownObj::getTimeout()
{
	return bye_timeout.getValue();
}

clustertlsconnectshutdownObj::clustertlsconnectshutdownObj()

{
}

clustertlsconnectshutdownObj::~clustertlsconnectshutdownObj() noexcept
{
}

std::string clustertlsconnectshutdownObj::getName() const
{
	return "tlsshutdown";
}


clustertlsconnectshutdownObj::mcguffin_destructor_cb
::mcguffin_destructor_cb(const STASHER_NAMESPACE::stoppableThreadTracker &trackerArg,
			 const x::ref<clustertlsconnectshutdownObj
				      > &shutdownArg,
			 const x::fd &socketArg,
			 const x::gnutls::session &sessionArg)
	: tracker(trackerArg), shutdown(shutdownArg),
	  socket(socketArg), session(sessionArg)
{
}

clustertlsconnectshutdownObj::mcguffin_destructor_cb
::~mcguffin_destructor_cb() noexcept
{
}

void clustertlsconnectshutdownObj::mcguffin_destructor_cb::destroyed()
{
	tracker->start_thread(shutdown, socket, session);
}

void clustertlsconnectshutdownObj
::create(const x::ptr<x::obj> &connection_mcguffin,
	 const x::ptr<STASHER_NAMESPACE::stoppableThreadTrackerObj> &tracker,
	 const x::fd &socket,
	 const x::gnutls::session &session,
	 const x::ptr<clustertlsconnectshutdownObj> &thr)

{
	auto cb=x::ref<mcguffin_destructor_cb>
		::create(tracker, thr, socket, session);

	connection_mcguffin->ondestroy([cb]{cb->destroyed();});
}

void clustertlsconnectshutdownObj::run(x::ptr<x::obj> &threadmsgdispatcher_mcguffin,
				       const x::fd &socket,
				       const x::gnutls::session &session)
{
	msgqueue_auto msgqueue(this);
	threadmsgdispatcher_mcguffin=nullptr;

	struct pollfd pfd[2];

	{
		x::fd fd(msgqueue->getEventfd());

		fd->nonblock(true);

		pfd[0].fd=fd->getFd();
	}
	pfd[0].events=POLLIN;
	pfd[1].fd=socket->getFd();

	LOG_INFO("Shutting down TLS connection");

	try {
		x::timespec now=x::timespec::getclock(CLOCK_MONOTONIC);

		x::timespec expiration=now + getTimeout();

		while (1)
		{
			int direction;

			if (session->bye(direction))
				return;

			while (!msgqueue->empty())
				msgqueue.event();

			now=x::timespec::getclock(CLOCK_MONOTONIC);

			if (now >= expiration)
				break;

			time_t sec=(expiration - now).tv_sec;

			if (sec <= 0)
				break;

			pfd[1].events=direction;

			if (poll(pfd, 2, sec * 1000) < 0)
				if (errno != EINTR)
					throw SYSEXCEPTION("poll");
		}
		LOG_ERROR("TLS shutdown timeout");
	} catch (const x::exception &e)
	{
		LOG_ERROR(e);
	}
}

/*
** Copyright 2012-2016 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "peerstatusannouncer.H"

MAINLOOP_IMPL(peerstatusannouncerObj)

void peerstatusannouncerObj::statusupdated(const nodeclusterstatus &newStatus)
{
	do_statusupdated(newStatus);
}

void peerstatusannouncerObj::initialstatus(const nodeclusterstatus
					   &newStatus)
{
	thisstatus=newStatus;
}

void peerstatusannouncerObj::sendthisstatus()
{
	writer->write(x::ref<STASHER_NAMESPACE::writtenObj<nodeclusterstatus> >
		      ::create(thisstatus));
}

void peerstatusannouncerObj::started()
{
	sendthisstatus();
}

void peerstatusannouncerObj::deserialized(const nodeclusterstatus &newStatus)

{
	peerstatusupdate(newStatus);
}

void peerstatusannouncerObj::dispatch_do_statusupdated(const nodeclusterstatus &newStatus)
{
	thisstatus=newStatus;

	sendthisstatus();
	thisstatusupdated();
}

void peerstatusannouncerObj::thisstatusupdated()
{
}

peerstatusannouncerObj::peerstatusannouncerObj(const std::string &peernameArg)

	: STASHER_NAMESPACE::fdobjrwthreadObj("write(" + peernameArg + ")"),
	  peerstatusObj(peernameArg)
{
}

peerstatusannouncerObj::~peerstatusannouncerObj() noexcept
{
}

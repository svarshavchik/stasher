/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "stasher/userput.H"
#include "stasher/userinit.H"
#include "objwriter.H"
#include "writtenobj.H"
#include "fdobjwriterthread.H"

STASHER_NAMESPACE_START

userput::userput(const x::ptr<puttransactionObj> &tranArg,
		 const userinit &limits)
	: tran(tranArg), chunksize(limits.chunksize)
{
}

userput::userput(const userinit &limits)
	: tran(x::ptr<puttransactionObj>::create()),
	  chunksize(limits.chunksize)
{
	tran->maxobjects=limits.maxobjects;
}

void userput::write(const x::ptr<fdobjwriterthreadObj> &wthread)

{
	wthread->write(x::ptr<writtenObj<userput> >::create(*this));

	for (std::vector<puttransactionObj::updinfo>::iterator
		     b=tran->objects.begin(), e=tran->objects.end();
	     b != e; ++b)
	{
		if (b->delflag)
			continue;

		{
			std::vector<x::fd> fd;

			x::fdptr fdp=b->contents->contents_filedesc;

			if (!fdp.null())
			{
				fd.push_back(fdp);
				wthread->sendfd(fd);
				continue;
			}
		}

		std::string::const_iterator cb=b->contents->contents.begin(),
			ce=b->contents->contents.end();

		do
		{
			std::string::const_iterator cp=
				(size_t)(ce-cb) < chunksize ? ce:
				cb+chunksize;

			wthread->write(x::ptr<writtenObj<puttransactionObj
							 ::content_str> >
				       ::create(std::string(cb, cp)));
			cb=cp;
		} while (cb != ce);
	}
}

STASHER_NAMESPACE_END

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "stasher/userput.H"
#include "localconnection.H"
#include "tobjrepo.H"
#include "newtran.H"
#include "userput_deserialized.H"
#include <x/exception.H>

#include <cstdio>

LOG_CLASS_INIT(STASHER_NAMESPACE::userput::deserializedObj);

STASHER_NAMESPACE_START

userput::userput(const localconnectionObj &conn)
	: tran(x::ptr<puttransactionObj>::create()),
	  chunksize(conn.limits.chunksize)
{
	tran->maxobjects=conn.limits.maxobjects;
}

userput::deserializedObj::deserializedObj(const x::ptr<puttransactionObj>
					  &puttranArg,
					  const userinit &limits,
					  const spacemonitor
					  &diskspaceArg,
					  const tobjrepo &repoArg,
					  const x::uuid &requuidArg,
					  const nsview &namespaceviewArg)
	: puttran(puttranArg),
	  repo(repoArg),
	  tran(repo->newtransaction()),
	  requuid(requuidArg),
	  namespaceview(namespaceviewArg),
	  errcode(req_processed_stat),
	  cur_object(0)
{
	long alloc_units=0;
	long alloc_inodes=0;

	std::set<std::string> names;

	for (auto putobj : puttran->objects)
	{
		std::string n=putobj.name;

		if (!namespaceview->fromclient_write(n))
			errcode=req_eperm_stat;
		else if (!names.insert(n).second)
		{
			errcode=req_dupeobj_stat;
		}

		if (putobj.delflag)
			continue;

		if (putobj.newflag)
			alloc_inodes += diskspaceArg->calculate_inode(putobj.name);

		if (putobj.contents_size < 0)
			throw EXCEPTION("Internal error: negative object size");
		alloc_units += diskspaceArg->calculate_alloc(putobj.contents_size);
	}

	if (puttran->totsize() > limits.maxobjectsize)
		throw EXCEPTION("Internal error: aggregate transaction size "
				"exceeds maximum");

	diskspace=diskspaceArg->reservespace_alloc(alloc_units, alloc_inodes);

	if (diskspaceArg->outofspace())
	{
		errcode=req_enomem_stat;
		return;
	}
	next_object();
}

userput::deserializedObj::~deserializedObj() noexcept
{
}

bool userput::deserializedObj::completed()
{
	return cur_object >= puttran->objects.size();
}

void userput::deserializedObj::received(const puttransactionObj::content_str
					&str)

{
	puttransactionObj::updinfo &obj=puttran->objects[cur_object];

	if (str.str.size() > (size_t)(obj.contents_size - cur_object_cnt))
		throw EXCEPTION("Internal error: object size mismatch (str)");

	if (errcode == req_processed_stat)
		cur_object_fd->write_full(str.str.c_str(), str.str.size());

	if ((cur_object_cnt += str.str.size()) == obj.contents_size)
	{
		if (errcode == req_processed_stat)
			cur_object_fd->close();
		++cur_object;
		next_object();
	}
}

void userput::deserializedObj::received(const x::fd &socket)

{
	if (cur_object_cnt > 0)
		throw EXCEPTION("Internal exception: received a file descriptor"
				" unexpectedly");

	puttransactionObj::updinfo &obj=puttran->objects[cur_object];

	if (errcode == req_processed_stat)
	{
		struct stat stat_buf= *socket->stat();

		if (!S_ISREG(stat_buf.st_mode) ||
		    stat_buf.st_size != obj.contents_size)
			throw EXCEPTION("Internal error: object size mismatch"
					" (fd)");
	}

	if (errcode == req_processed_stat)
	{
		off_t cnt=obj.contents_size;
		off_t start_pos=0;

		char buffer[BUFSIZ];

		while (cnt > 0)
		{
			size_t n=sizeof(buffer);

			if ((off_t)n > cnt)
				n=(size_t)cnt;

			ssize_t i=socket->pread(start_pos, buffer, n);

			if (i <= 0)
				throw EXCEPTION("Internal: error reading object data file descriptor");
			start_pos += i;

			cur_object_fd->write_full(buffer, i);
			cnt -= i;
		}
		cur_object_fd->close();
	}
	++cur_object;
	next_object();
}

void userput::deserializedObj::next_object()
{
	for (; cur_object < puttran->objects.size(); ++cur_object)
	{
		puttransactionObj::updinfo &obj=
			puttran->objects[cur_object];

		std::string name;

		try
		{
			name=obj.name;

			if (!namespaceview->fromclient_write(name))
				errcode=req_eperm_stat;
			else
			{
				char buf[repo->obj_name_len(name)];

				repo->obj_name_create(name, buf);
			}
		} catch (const x::exception &e)
		{
			errcode=req_badname_stat;
		}

		if (!obj.delflag)
		{
			if (errcode == req_processed_stat)
				try {
					cur_object_fd=
						obj.newflag ?
						tran->newobj(name)
						:
						tran->updobj(name,
							     obj.uuid);
				} catch (const x::exception &e)
				{
					errcode=req_failed_stat;
					LOG_ERROR(e);
					LOG_TRACE(e->backtrace);
				}
			cur_object_cnt=0;
			break;
		}
		if (errcode == req_processed_stat)
			try {
				tran->delobj(name, obj.uuid);
			} catch (const x::exception &e)
			{
				errcode=req_failed_stat;
				LOG_ERROR(e);
				LOG_TRACE(e->backtrace);
			}
	}
}

STASHER_NAMESPACE_END

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "stasher/puttransaction.H"
#include "stasher/userput.H"
#include "objwriter.H"
#include "writtenobj.H"

#include <cstdio>

STASHER_NAMESPACE_START

puttransactionObj::objcontentsObj::objcontentsObj(const x::fd &filedesc)
 : contents_filedesc(filedesc)
{
}

puttransactionObj::objcontentsObj::objcontentsObj(const std::string
						  &contentsArg)
	: contents(contentsArg)
{
}
		
puttransactionObj::objcontentsObj::~objcontentsObj()
{
}

puttransactionObj::objcontents::objcontents()
{
}

puttransactionObj::objcontents::~objcontents() {}

puttransactionObj::objcontents::objcontents(const x::fd &filedesc)
	: x::ptr<objcontentsObj>(x::ptr<objcontentsObj>::create(filedesc))
{
}

puttransactionObj::objcontents::objcontents(const std::string &str)
	: x::ptr<objcontentsObj>(x::ptr<objcontentsObj>::create(str))
{
}

puttransactionObj::updinfo::updinfo(const std::string &nameArg,
				    const objcontents &contentsArg)
	: name(nameArg), newflag(true), delflag(false),
	  contents(contentsArg)
{
	setsize();
}

puttransactionObj::updinfo::updinfo(const std::string &nameArg,
				    const x::uuid &uuidArg,
				    const objcontents &contentsArg)
 : name(nameArg), newflag(false), uuid(uuidArg),
			      delflag(false), contents(contentsArg)
{
	setsize();
}

puttransactionObj::updinfo::updinfo(const std::string &nameArg,
				    const x::uuid &uuidArg)
	: name(nameArg), newflag(false), uuid(uuidArg),
	  delflag(true), contents_size(0)
{
}

puttransactionObj::updinfo::updinfo()
	: newflag(false), delflag(false), contents_size(0)
{
}

void puttransactionObj::updinfo::setsize()
{
	x::fdptr fd=contents->contents_filedesc;

	if (fd.null())
	{
		contents_size=contents->contents.size();
		return;
	}

	struct stat stat_buf= *contents->contents_filedesc->stat();

	contents_size=stat_buf.st_size;
}

puttransactionObj::updinfo::~updinfo()
{
}

puttransactionObj::puttransactionObj() : admin(false),
							     maxobjects(0)
{
}

puttransactionObj::~puttransactionObj()
{
}

bool puttransactionObj::empty()
{
	return objects.empty();
}

off_t puttransactionObj::totsize() const
{
	off_t sum=0;

	for (std::vector<updinfo>::const_iterator
		     b=objects.begin(), e=objects.end(); b != e; ++b)
	{
		if (b->delflag)
			continue;

		if (sum + b->contents_size < sum)
			throw EXCEPTION("Object size overflow");

		sum += b->contents_size;
	}
	return sum;
}


void puttransactionObj::newobj_impl(const std::string &objname,
				    const objcontents &contents)

{
	objects.push_back(updinfo(objname, contents));
}


void puttransactionObj::updobj_impl(const std::string &objname,
				    const x::uuid &uuid,
				    const objcontents &contents)

{
	objects.push_back(updinfo(objname, uuid, contents));
}

void puttransactionObj::delobj(const std::string &objname,
			       const x::uuid &uuid)
{
	objects.push_back(updinfo(objname, uuid));
}

STASHER_NAMESPACE_END

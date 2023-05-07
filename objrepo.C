/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "objrepo.H"
#include "stasher/objname.H"
#include <x/uuid.H>
#include <x/fileattr.H>
#include <x/sysexception.H>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <iterator>

#define TMP "tmp"
#define DATA "data"

LOG_CLASS_INIT(objrepoObj);

#define XATTRSERIAL "user.serial"

const char objrepoObj::xattrserial[]=XATTRSERIAL;

objrepoObj::objrepoObj(const std::string &directoryArg)
	: directory(directoryArg),
	  lock((mkdir(directoryArg.c_str(), 0700),
		x::fd::base::open(directory + "/.lock", O_RDWR | O_CREAT,
				  0666))),
	  notifiers(notifier_list_t::create())
{
	if (!lock->lockf(F_TLOCK))
		throw EXCEPTION("Server already started");

	std::string data_name(directory + "/" DATA);

	mkdir((directory + "/" TMP).c_str(), 0777);
	mkdir(data_name.c_str(), 0777);

	LOG_INFO("Validating repository");

	std::vector<std::string> directories;

	for (auto &data_dir :
		     std::filesystem::recursive_directory_iterator{data_name})
	{
		std::string p=data_dir.path();

		if (data_dir.is_directory())
		{
			directories.push_back(std::move(p));
			continue;
		}

		std::string objname=fullpath_to_objname(p);

		try {
			char n[obj_name_len(objname)];

			obj_name_create(objname, n);

			if (p == n)
			{
				// TODO: gcc bug?
				x::uuid uuid(x::fileattr::create(&n[0])->
					     getattr(xattrserial));
				continue;
			}
		} catch (...) {
		}

		LOG_ERROR(p << ": invalid object, removed");

		unlink(p.c_str());
	}

	std::sort(directories.begin(), directories.end(),
		  [](const auto &a, const auto &b)
		  {
			  return b < a;
		  });

	for (auto &p:directories)
	{
		if (p != data_name)
		{
			if (rmdir(p.c_str()) == 0)
			{
				LOG_INFO("Removed empty directory: "
					 << p);
			}
		}
	}

	for (auto &tmp_iter : std::filesystem::directory_iterator{
			directory + "/" TMP})
	{
		try {
			valid_tmp_name(tmp_iter.path().filename());
			continue;
		} catch (...) {
		}

		auto n=tmp_iter.path();

		LOG_ERROR(n << ": invalid filename, removed");
		unlink(n.c_str());
	}

	LOG_INFO("Validated repository");
}

objrepoObj::~objrepoObj()
{
}

std::string objrepoObj::valid_tmp_name(const std::string &s)
{
	if (s.empty() || s[0] == '.' ||
	    s.find('/') != std::string::npos)
		throw EXCEPTION("Internal error -- invalid temporary filename");

	return directory + "/" TMP "/" + s;
}

x::fd objrepoObj::tmp_open(const std::string &tmpfilename,
			   int flags,
			   mode_t openmode)
{
	return x::fd::base::open(valid_tmp_name(tmpfilename), flags, openmode);
}

x::fileattr objrepoObj::tmp_get(const std::string &tmpfilename)

{
	return x::fileattr::create(valid_tmp_name(tmpfilename));
}

bool objrepoObj::tmp_exists(const std::string &tmpfilename)
{
	struct stat stat_buf;

	return ::stat(valid_tmp_name(tmpfilename).c_str(), &stat_buf) == 0;
}

void objrepoObj::tmp_rename(const std::string &oldname,
			    const std::string &newname)
{
	std::shared_lock<std::shared_mutex> r{tmp_writelock};

	std::string o(valid_tmp_name(oldname)),
		n(valid_tmp_name(newname));

	if (rename(o.c_str(), n.c_str()) < 0)
		throw SYSEXCEPTION("rename(\"" << oldname << "\", \"" <<
				   newname << "\")");
}

void objrepoObj::tmp_link(const std::string &oldname,
			  const std::string &newname)
{
	std::string o(valid_tmp_name(oldname)),
		n(valid_tmp_name(newname));

	if (link(o.c_str(), n.c_str()) < 0)
		throw SYSEXCEPTION("link(\"" + oldname + "\", \"" +
				   newname + "\")");
}

void objrepoObj::tmp_remove(const std::string &tmpfilename)
{
	std::shared_lock<std::shared_mutex> r{tmp_writelock};

	unlink(valid_tmp_name(tmpfilename).c_str());
}

std::tuple<objrepoObj::tmp_iter_t,
	  objrepoObj::tmp_iter_t> objrepoObj::tmp_iter()
{
	std::filesystem::directory_iterator d{directory + "/" TMP};

	return { begin(d), end(d) };
}

size_t objrepoObj::obj_name_len(const std::string &s)
{
	if (s.size() > maxnamesize)
		throw EXCEPTION("Object name too large");

	return directory.size() + sizeof(DATA) + 4
		+ STASHER_NAMESPACE
		::encoded_object_name_length(s.begin(), s.end());
}

void objrepoObj::obj_name_create(const std::string &s, char *p)

{
	std::string t(directory + "/" DATA "/");

	*STASHER_NAMESPACE::encode_object_name(s.begin(),
					       s.end(),
					       std::copy(t.begin(),
							 t.end(),
							 p))=0;
}

void objrepoObj::obj_install(const std::string &tmpfilename,
			     const std::string &objname,
			     const x::ptr<x::obj> &lock)
{
	std::string tname(valid_tmp_name(tmpfilename));

	const char *tname_str=tname.c_str();

	char dstfilename[obj_name_len(objname)];

	obj_name_create(objname, dstfilename);

	LOG_DEBUG("Installing " << dstfilename);
	while (rename(tname_str, dstfilename))
	{
		struct ::stat stat_buf_dummy;

		if (errno != ENOENT)
			throw SYSEXCEPTION(&dstfilename[0]);

		if (::stat(tname_str, &stat_buf_dummy))
		{
			if (errno == ENOENT)
			{
				LOG_TRACE(tname_str
					  << " doesn't exist, ignoring");
				return; // Already renamed
			}
			throw SYSEXCEPTION(tname_str);
		}

		LOG_TRACE("Intermediate directory missing. Recreating");

		// Some missing directory component.

		char *e=dstfilename+strlen(dstfilename);
		char *b=dstfilename+directory.size();

		char *p=e;

		while (p > b)
		{
			if (*--p == '/')
			{
				*p=0;
#ifdef MKDIRING_DEBUG_HOOK
				MKDIRING_DEBUG_HOOK(dstfilename+directory.size()
						    +sizeof("/" DATA));
#endif

				if (mkdir(dstfilename, 0777) == 0
				    || errno == EEXIST)
				{
#ifdef MKDIRED_DEBUG_HOOK
					MKDIRED_DEBUG_HOOK(dstfilename
							   +directory.size()
							   +sizeof("/" DATA));
#endif
					LOG_TRACE(dstfilename << " created");

					// Created some parent directory,
					// now work forward.
					*p='/';
					while (p < e)
					{
						if (*++p == 0)
						{
							if (p == e)
								break;

#ifdef MKDIRING_DEBUG_HOOK
							MKDIRING_DEBUG_HOOK(dstfilename
									    +directory.size()
									    +sizeof("/" DATA));
#endif
							if (mkdir(dstfilename,
								  0777) == 0
							    || errno==EEXIST
							    )
							{
#ifdef MKDIRED_DEBUG_HOOK
								MKDIRED_DEBUG_HOOK(dstfilename
										   +directory.size()
										   +sizeof("/" DATA));
#endif

								LOG_TRACE(dstfilename
									  << " created");
								*p='/';
								continue;
							}
							if (errno == ENOENT)
								// Race with someone rmdir-ing it
								break;
							throw SYSEXCEPTION(b);
						}
					}
					if (p == e)
						break;
				}
				if (errno == EEXIST)
					break;
				if (errno != ENOENT)
					throw SYSEXCEPTION(b);
				LOG_TRACE(dstfilename << " does not exist");
			}
		}

		if (p == b)
			throw EXCEPTION(dstfilename
					+ std::string(": cannot create"));
	}
	LOG_DEBUG("Installed: " << dstfilename);

	for (auto notifierp: *notifiers)
	{
		notifier n(notifierp.getptr());

		if (!n.null())
			n->installed(objname, lock);
	}
}

void objrepoObj::obj_remove(const std::string &objname,
			    const x::ptr<x::obj> &lock)
{
	char tmpfilename[obj_name_len(objname)];

	obj_name_create(objname, tmpfilename);

	LOG_DEBUG("Removing " << tmpfilename);

	if (unlink(tmpfilename) && errno != ENOENT)
		throw SYSEXCEPTION(&tmpfilename[0]);

	// ENOENT -- file was already removed, rerunning the transaction

	char *e=tmpfilename+strlen(tmpfilename);
	char *b=tmpfilename+directory.size() + sizeof("/" DATA);

	char *p=e;

	while (p > b)
	{
		if (*--p != '/')
			continue;

		*p=0;

#ifdef RMDIRING_DEBUG_HOOK
		RMDIRING_DEBUG_HOOK(tmpfilename+directory.size()
				    +sizeof("/" DATA));
#endif
		if (rmdir(tmpfilename) == 0)
		{
			LOG_TRACE(tmpfilename << " removed");
			continue;
		}

#ifdef RMDIRED_DEBUG_HOOK
		RMDIRED_DEBUG_HOOK(tmpfilename+directory.size()
				   +sizeof("/" DATA));
#endif

		if (errno == ENOENT)
		{
			LOG_TRACE(tmpfilename << " does not exist");
			break;
		}

		if (errno == ENOTEMPTY)
		{
			LOG_TRACE(tmpfilename << " not empty");
			break;
		}
		throw SYSEXCEPTION(&tmpfilename[0]);
	}

	for (auto notifierp:*notifiers)
	{
		notifier n(notifierp.getptr());

		if (!n.null())
			n->removed(objname, lock);
	}
}

bool objrepoObj::obj_exists(const std::string &objname)
{
	char tmpfilename[obj_name_len(objname)];

	obj_name_create(objname, tmpfilename);

	struct stat stat_buf;

	return ::stat(tmpfilename, &stat_buf) == 0;
}

x::fileattr objrepoObj::obj_get(const std::string &objname)
{
	char tmpfilename[obj_name_len(objname)];

	obj_name_create(objname, tmpfilename);

	// TODO: gcc bug?
	return x::fileattr::create(&tmpfilename[0]);
}

x::fdptr objrepoObj::obj_open(const std::string &objname)
{
	char tmpfilename[obj_name_len(objname)];

	obj_name_create(objname, tmpfilename);

	try {
		return x::fd::base::open(tmpfilename, O_RDONLY);
	} catch (const x::sysexception &e)
	{
		if (e.getErrorCode() != ENOENT)
			throw;
	}
	return x::fdptr();
}

void objrepoObj::values(const std::set<std::string> &objects,
			bool openflag,
			values_t &retvalues,
			std::set<std::string> &notfound)
{
	for (std::set<std::string>::const_iterator
		     b(objects.begin()),
		     e(objects.end()); b != e; ++b)
	{
		x::fdptr fd;
		x::uuid uuid( ({
					std::string s;

					if (openflag)
					{
						fd=obj_open(*b);
						if (fd.null())
						{
							notfound.insert(*b);
							continue;
						}

						s=fd->getattr(xattrserial);
					}
					else try {
							s=obj_get(*b)->getattr
								(xattrserial);
						} catch (const x::sysexception
							 &e)
						{
							if (e.getErrorCode()
							    == ENOENT)
							{
								notfound.insert
									(*b);
								continue;
							}
						}

					s;
				}) );

		retvalues.insert(std::make_pair(*b, std::make_pair(uuid, fd)));
	}
}

std::string objrepoObj::topdir(const std::string &hier)
{
	if (hier.size() == 0)
		return directory + "/" DATA;

	char tmpfilename[obj_name_len(hier)];

	obj_name_create(hier, tmpfilename);

	std::string topdir=tmpfilename;
	return topdir.substr(0, topdir.size()-2); // Chop off the .f
}


objrepoObj::obj_iter_t objrepoObj::obj_begin(const std::string &hier)

{
	std::error_code ec;

	auto dw=std::filesystem::recursive_directory_iterator{
		topdir(hier),
		{},
		ec};

	return obj_iter_t(objrepo(this), begin(dw), end(dw));
}

objrepoObj::obj_iter_t objrepoObj::obj_end()
{
	return obj_iter_t();
}

objrepoObj::dir_iter_t objrepoObj::dir_begin(const std::string &hier)

{
	std::error_code ec;

	auto dw=std::filesystem::directory_iterator{topdir(hier), {}, ec};

	return dir_iter_t(objrepo(this), begin(dw), end(dw));
}

objrepoObj::dir_iter_t objrepoObj::dir_end()
{
	return dir_iter_t();
}

// Full path to some file entry in DATA, decode the pathname.

std::string objrepoObj::fullpath_to_objname(const std::string &s) noexcept
{
	std::string n;

	size_t startPos=directory.size() + sizeof("/" DATA);

	n.reserve(s.size()-startPos);

	for (std::string::const_iterator sb(s.begin()+startPos),
		     se(s.end()); sb != se; )
	{
		if (*sb == '\\')
		{
			if (++sb != se)
			{
				switch (*sb) {
				case '_':
					n += '.';
					break;
				default:
					n += *sb;
					break;
				}
				++sb;
			}
		}
		else
		{
			if (*sb == '.')
				break; /* Reached the .f suffix */
			n += *sb;
			++sb;
		}
	}

	return n;
}

objrepoObj::obj_iter_t::obj_iter_t(
	const objrepo &repo,
	const std::filesystem::recursive_directory_iterator &b,
	const std::filesystem::recursive_directory_iterator &e)
	: repo{repo}, b{b}, e{e}
{
	n_init();
}

objrepoObj::obj_iter_t::obj_iter_t()
{
}

void objrepoObj::obj_iter_t::n_init()
{
	while (b != e)
	{
		if (!b->is_directory())
		{
			n=repo->fullpath_to_objname(b->path());
			return;
		}

		++b;
	}

	b=e={};
	repo=objrepoptr();
}

objrepoObj::obj_iter_t &objrepoObj::obj_iter_t::operator++()

{
	if (!repo.null())
	{
		++b;
		n_init();
	}
	return *this;
}

objrepoObj::dir_iter_t::dir_iter_t(
	const objrepo &repo,
	const std::filesystem::directory_iterator &b,
	const std::filesystem::directory_iterator &e)
	: repo{repo}, b{b}, e{e}
{
	n_init();
}

objrepoObj::dir_iter_t::dir_iter_t()
{
}

void objrepoObj::dir_iter_t::n_init()
{
	if (b != e)
	{
		std::string s=b->path();

		n=repo->fullpath_to_objname(s);

		if (s.size() > 2 && s.substr(s.size()-2) != ".f")
			n += "/";
		return;
	}

	b=e={};
	repo=objrepoptr();
}

objrepoObj::dir_iter_t &objrepoObj::dir_iter_t::operator++()

{
	if (!repo.null())
	{
		++b;
		n_init();
	}
	return *this;
}

objrepoObj::notifierObj::notifierObj()
{
}

objrepoObj::notifierObj::~notifierObj()
{
}

void objrepoObj::installNotifier(const notifier &n)
{
	notifiers->push_back(n);
}

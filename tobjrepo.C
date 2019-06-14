/*
** Copyright 2012-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "tobjrepo.H"
#include "newtran.H"
#include "trancommit.H"
#include "tran.H"

#include <x/serialize.H>
#include <x/deserialize.H>
#include <x/fd.H>
#include <x/sysexception.H>

#include <iterator>
#include <algorithm>
#include <sstream>

#define T_SUFFIX "t" /* Completed transaction */
#define C_SUFFIX "c" /* Committing transaction */
#define X_SUFFIX "x" /* Cancelling a transaction */

LOG_CLASS_INIT(tobjrepoObj);

const char tobjrepoObj::node_opt[]="NODE";

tobjrepoObj::uuidlock::uuidlock(tobjrepoObj *pArg,
				const x::uuid &uuidArg)
	: p(pArg)
{
	if (p == NULL)
		return;

	locked_uuids_container_t::lock lock(p->locked_uuids);

	while (1)
	{
		std::pair<locked_uuids_t::iterator, bool> iter=
			lock->insert(uuidArg);

		if (iter.second)
		{
			lockiter=iter.first;
			break;
		}
		lock.wait();
	}
}

tobjrepoObj::uuidlock::~uuidlock()
{
	if (p == NULL)
		return;

	locked_uuids_container_t::lock lock(p->locked_uuids);

	lock->erase(lockiter);
	lock.notify_all();
}

tobjrepoObj::tobjrepoObj(const std::string &directoryArg)
	: objrepoObj(directoryArg), lockpool(lockpool_t::create()),
	  commitlockpool(commitlockpool_t::create())
{
	LOG_INFO("Checking open transactions");

	{
		std::map<std::string, bool> unfinished_business;

		for (std::pair<tmp_iter_t, tmp_iter_t> iter(tmp_iter());
		     iter.first != iter.second; ++iter.first)
		{
			std::string n(*iter.first);
			size_t p=n.find('.');

			if (p == std::string::npos)
				continue;

			std::string suffix(n.substr(p+1));

			if (suffix == C_SUFFIX)
				unfinished_business[n]=false;
			else if (suffix == X_SUFFIX)
				unfinished_business[n]=true;
		}

		for (std::map<std::string, bool>::const_iterator
			     b=unfinished_business.begin(),
			     e=unfinished_business.end(); b != e; ++b)
		{
			try {
				auto tranRef=tran::create(x::uuid
							  {b->first.substr
								   (0,
								    b->first
								    .find('.')
								    )});

				parse(*tranRef, tmp_open(b->first, O_RDONLY));

				if (b->second)
				{
					LOG_INFO("Cancelling " <<
						 x::to_string(tranRef->uuid));
					cancel_uuid_locked(tranRef);
				}
				else
				{
					std::string uuid_str(x::to_string(tranRef->uuid));

					LOG_INFO("Committing " << uuid_str);

					commit_uuid_locked(uuid_str, *tranRef,
							   x::ptr<x::obj>());
				}
			} catch (const x::exception &e)
			{

			}
		}
	}

	{
		std::set<std::string> removeset;

		for (std::pair<tmp_iter_t, tmp_iter_t> iter(tmp_iter());
		     iter.first != iter.second; ++iter.first)
		{
			std::string n(*iter.first);
			size_t p=n.find('.');

			if (p != std::string::npos)
			{
				std::string suffix(n.substr(p+1));

				if (suffix == T_SUFFIX)
					continue;

				std::istringstream i(suffix);
				size_t dummy;

				i >> dummy;

				if (!i.fail())
				{
					if (tmp_exists(n.substr(0, p) +
						       "." T_SUFFIX))
						continue;
				}
			}

			removeset.insert(n);
		}

		for (std::set<std::string>::iterator b(removeset.begin()),
			     e(removeset.end()); b != e; ++b)
		{
			LOG_ERROR("Removing " << *b);

			objrepoObj::tmp_remove(*b);
		}
	}
}

tobjrepoObj::~tobjrepoObj()
{
}

x::fd tobjrepoObj::tmp_create(const x::uuid &uuid, size_t objnum)

{
	std::ostringstream o;

	o << x::to_string(uuid) << '.' << objnum;

	x::fd fd(tmp_open(o.str(), O_CREAT|O_RDWR));

	tmp_set_uuid(fd, uuid);

	return fd;
}

x::fd tobjrepoObj::tmp_reopen(const x::uuid &uuid, size_t objnum)

{
	std::ostringstream o;

	o << x::to_string(uuid) << '.' << objnum;

	x::fd fd(tmp_open(o.str(), O_RDONLY));

	return fd;
}

void tobjrepoObj::tmp_remove(const x::uuid &uuid, size_t objnum)

{
	std::ostringstream o;

	o << x::to_string(uuid) << '.' << objnum;

	objrepoObj::tmp_remove(o.str());
}

void tobjrepoObj::tmp_set_uuid(const x::fd &tmp_fd, const x::uuid &uuid)

{
	x::uuid::charbuf cb;

	uuid.asString(cb);

	tmp_fd->setattr(xattrserial, cb);
}

newtran tobjrepoObj::newtransaction()
{
	return newtran(newtran::create(tobjrepo(this)));
}

newtran tobjrepoObj::newtransaction(const x::uuid &uuidArg)
{
	return newtran(newtran::create(tobjrepo(this), uuidArg));
}

void tobjrepoObj::finalize(const newtran &tran)
{
	x::uuid::charbuf cb;

	tran->uuid.asString(cb);

	x::fd fd(tmp_open(cb, O_CREAT|O_RDWR));

	x::ostream o(fd->getostream());

	x::streambuf obuf(o->rdbuf());

	{
		std::ostreambuf_iterator<char> oiter(&*obuf);

		x::serialize
			::iterator< std::ostreambuf_iterator<char> >
			iter(oiter);

		iter(tran->meta);
	}

	obuf->pubsync();

	if (o->fail() || o->bad())
		throw SYSEXCEPTION("write");

	fd->close();

	std::ostringstream tname;

	tname << cb << "." T_SUFFIX;

	tmp_rename(cb, tname.str());
}

void tobjrepoObj::enumerate(finalized_cb &cbArg)
{
	{
		failedlist_container_t::lock lock(failedlist);

		for (auto iter: *lock)
			cbArg(iter.first, iter.second);
	}

	std::list<std::pair<x::uuid, std::string> > flist;

	{
		std::unique_lock<std::shared_mutex> w{tmp_writelock};

		for (std::pair<tmp_iter_t, tmp_iter_t> iter(tmp_iter());
		     iter.first != iter.second; ++iter.first)
		{
			std::string n=*iter.first;

			size_t p=n.find('.');

			if (p == std::string::npos)
				continue;

			if (n.substr(p) != "." T_SUFFIX)
				continue;

			bool have_uuid=false;

			try {
				x::uuid uuid(n.substr(0, p));

				have_uuid=true;

				flist.push_back
					(std::make_pair
					 (uuid,
					  ({
						  tranmeta meta;

						  parse(meta,
							open_tran_uuid_locked
							(uuid));
						  meta.opts[tobjrepoObj
							    ::node_opt];
					  })));
			} catch (const x::exception &e)
			{
				if (have_uuid)
					throw;
			}
		}

	}

	while (!flist.empty())
	{
		std::pair<x::uuid, std::string> &p=flist.front();

		cbArg(p.first, dist_received_status_t(dist_received_status_ok,
						      p.second));
		flist.pop_front();
	}
}

void tobjrepoObj::parse(tranmeta &tranArg,
			const x::fd &fdArg)
{
	x::istream i(fdArg->getistream());

	x::streambuf ibuf(i->rdbuf());

	std::istreambuf_iterator<char> iter(&*ibuf), iter_end;

	x::deserialize
		::iterator< std::istreambuf_iterator<char> >
		ser_iter(iter, iter_end);

	ser_iter(tranArg);
}

void tobjrepoObj::cancel(const x::uuid &uuidArg)
{
	auto tranRef=tran::create(uuidArg);

	uuidlock ulock(this, uuidArg);

	{
		failedlist_container_t::lock lock(failedlist);

		failedlist_t::iterator iter=lock->find(uuidArg);

		if (iter != lock->end())
		{
			LOG_ERROR("Failed transaction: "
				  << x::to_string(uuidArg)
				  << ": removed");

			lock->erase(iter);
			return;
		}
	}


	std::string uuidstr=x::to_string(uuidArg);

	std::string tname=uuidstr + "." T_SUFFIX;
	std::string xname=uuidstr + "." X_SUFFIX;

	tmp_rename(tname, xname);

	parse(*tranRef, tmp_open(xname, O_RDONLY));

#ifdef CANCEL_DEBUG_HOOK
	CANCEL_DEBUG_HOOK(uuidArg);
#endif

	cancel_uuid_locked(tranRef);
}

void tobjrepoObj::cancel_uuid_locked(const tran &tranArg)
{
	tranmeta &meta= *tranArg;

	std::string uuid(x::to_string(tranArg->uuid));

	for (size_t n=meta.objects.size(); n > 0; )
	{
		--n;

		if (!meta.objects[n].has_new_value())
			continue;

		std::ostringstream o;

		o << uuid << '.' << n;

		objrepoObj::tmp_remove(o.str());
	}

	std::map<std::string, std::string>::const_iterator
		node(meta.opts.find(node_opt));

	if (node != meta.opts.end())
		cancel_done(node->second, tranArg->uuid);

	objrepoObj::tmp_remove(uuid + "." X_SUFFIX);

}

trancommit tobjrepoObj::begin_commit(const x::uuid &uuidArg,
				     const x::eventfd &fdArg)

{
	auto c=trancommit::create(uuidArg, tobjrepo(this));

	{
		{
			uuidlock ulock(this, uuidArg);

			parse(*c, open_tran_uuid_locked(uuidArg));
		}

		std::set<std::string> objects;

		for (std::vector<tranmeta::objinfo>::const_iterator
			     b(c->objects.begin()),
			     e(c->objects.end()); b != e; ++b)
			if (!objects.insert(b->name).second)
				throw EXCEPTION(b->name + ": duplicate object in the transaction");

		c->lock=lockpool->addLockSet(objects, fdArg);
	}

	c->commitlock=commitlockpool->addLockSet(false, fdArg);
	return c;
}

void tobjrepoObj::commit_nolock(const x::uuid &uuidArg)

{
	uuidlock ulock(this, uuidArg);

	auto t=tran::create(uuidArg);

	try {
		parse(*t, open_tran_uuid_locked(uuidArg));
	} catch (...) {

		LOG_FATAL("Repository corruption detected: "
			  << x::to_string(uuidArg));
		throw;
	}
	commit_uuid_locked(uuidArg, *t, x::ptr<x::obj>());
}

x::fd tobjrepoObj::open_tran_uuid_locked(const x::uuid &uuidArg)

{
	std::ostringstream tname;

	tname << x::to_string(uuidArg) << "." T_SUFFIX;

	return tmp_open(tname.str(), O_RDONLY);
}

tobjrepoObj::commitlock_t tobjrepoObj::commitlock(const x::eventfd &fdArg)

{
	return commitlockpool->addLockSet(true, fdArg);
}

void tobjrepoObj::commit(const x::uuid &uuidArg, const tranmeta &tranArg,
			 const x::ptr<x::obj> &lock)

{
	uuidlock ulock(this, uuidArg);

	commit_uuid_locked(uuidArg, tranArg, lock);
}

void tobjrepoObj::commit_uuid_locked(const x::uuid &uuidArg,
				     const tranmeta &tranArg,
				     const x::ptr<x::obj> &lock)

{
	std::string uuid_str(x::to_string(uuidArg));

	try {
		tmp_link(uuid_str + "." T_SUFFIX,
			 uuid_str + "." C_SUFFIX);
	} catch (const x::sysexception &e)
	{
		if (e.getErrorCode() != EEXIST)
			throw;
	}

#ifdef COMMIT_DEBUG_HOOK
	COMMIT_DEBUG_HOOK(uuidArg);
#endif

	commit_uuid_locked(uuid_str, tranArg, lock);
}

void tobjrepoObj::commit_uuid_locked(const std::string &uuidArg,
				     const tranmeta &meta,
				     const x::ptr<x::obj> &lock)

{
	for (size_t n=meta.objects.size(); n > 0; )
	{
		--n;

		if (meta.objects[n].has_new_value())
		{
			std::ostringstream o;

			o << uuidArg << "." << n;

			obj_install(o.str(), meta.objects[n].name, lock);
		}
		else
			obj_remove(meta.objects[n].name, lock);
	}
	objrepoObj::tmp_remove(uuidArg + "." C_SUFFIX);

	std::map<std::string, std::string>::const_iterator
		node(meta.opts.find(node_opt));

	if (node == meta.opts.end())
		return;

	mark_done_uuid_locked(x::uuid{uuidArg}, node->second,
			      STASHER_NAMESPACE::req_processed_stat, lock);
}

const char tobjrepoObj::done_hier[]="etc/done";
const size_t tobjrepoObj::done_hier_l=8; // 8 characters

std::string tobjrepoObj::source_done_hier(const std::string &source)

{
	return std::string(done_hier) + "/" + source;
}

void tobjrepoObj::mark_done(const x::uuid &uuidArg,
			    const std::string &source,
			    STASHER_NAMESPACE::req_stat_t value,
			    const x::ptr<x::obj> &lockArg)
{
	uuidlock ulock(this, uuidArg);

	mark_done_uuid_locked(uuidArg, source, value, lockArg);
}

void tobjrepoObj::mark_done_uuid_locked(const x::uuid &uuidArg,
					const std::string &source,
					STASHER_NAMESPACE::req_stat_t value,
					const x::ptr<x::obj> &lockArg)

{
	std::string done_object_name=source_done_hier(source) + "/" +
		x::to_string(uuidArg);

	bool failed;

	{
		failedlist_container_t::lock lock(failedlist);

		failedlist_t::iterator iter=lock->find(uuidArg);

		failed=iter != lock->end();
	}

	if (failed)
	{
		LOG_ERROR("Failed transaction: " << x::to_string(uuidArg)
			  << ": does not need to be marked as done");

		// Note -- transactions never fail on the distributing node,
		// since otherwise they cannot be distributed, and only the
		// distributing node wants to be notified by the done files.
		return;
	}

	std::string tmp_name=x::to_string(uuidArg) + ".commit";

	x::fd fd(tmp_open(tmp_name, O_CREAT|O_RDWR));

	(*fd->getostream()) << (int)value << std::flush;
	tmp_set_uuid(fd, uuidArg);
	fd->close();

	obj_install(tmp_name, done_object_name, lockArg);
}

x::fdptr tobjrepoObj::open_tran_stat(const std::string &source,
				  const x::uuid &uuidArg)

{
	std::string n=source_done_hier(source) + "/" +
		x::to_string(uuidArg);

	std::set<std::string> objects;
	values_t values;
	std::set<std::string> notfound;

	objects.insert(n);

	this->values(objects, true, values, notfound);

	values_t::iterator iter(values.find(n));

	if (iter != values.end())
		return iter->second.second;

	return x::fdptr();
}

STASHER_NAMESPACE::req_stat_t
tobjrepoObj::get_tran_stat(const std::string &source,
			   const x::uuid &uuidArg)

{
	STASHER_NAMESPACE::req_stat_t val=STASHER_NAMESPACE::req_failed_stat;

	x::fdptr fd=open_tran_stat(source, uuidArg);

	if (!fd.null())
	{
		int n=(int)STASHER_NAMESPACE::req_failed_stat;

		(*fd->getistream()) >> n;

		val=(STASHER_NAMESPACE::req_stat_t)n;
	}

	return val;
}

void tobjrepoObj::cancel_done(const std::string &source,
			      const x::uuid &uuid)
{
	objrepoObj::obj_remove(source_done_hier(source) + "/" +
			       x::to_string(uuid), x::ptr<x::obj>());
}

void tobjrepoObj::cancel_done(const std::string &source)
{
	std::list<std::string> objnames;

	std::copy(obj_begin(source_done_hier(source)), obj_end(),
		  std::back_insert_iterator<std::list<std::string> >(objnames));

	for (std::list<std::string>::iterator b(objnames.begin()),
		     e(objnames.end()); b != e; ++b)
		objrepoObj::obj_remove(*b, x::ptr<x::obj>());
}

tobjrepoObj::lockentry_t tobjrepoObj::lock(const std::set<std::string> &objects,
					   const x::eventfd &fdArg)

{
	return lockpool->addLockSet(objects, fdArg);
}

void tobjrepoObj::failedlist_insert(const x::uuid &uuidArg,
				    const dist_received_status_t &statusArg)

{
	failedlist_container_t::lock lock(failedlist);

	lock->insert(std::make_pair(uuidArg, statusArg));

	LOG_ERROR("Failed transaction: " << x::to_string(uuidArg));
}

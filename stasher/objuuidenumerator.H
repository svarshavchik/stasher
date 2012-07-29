/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objuuidenumerator_H
#define objuuidenumerator_H

#include "tobjrepo.H"

#include <x/ref.H>
#include <x/weakptr.H>

class objuuidlistObj;

//! Enumerate uuids of objects in a transactional object repository

//! The constructor receives tobjrepo.
//!
//! Each call to next() returns an objuuidlist. A null reference indicates
//! the end of enumerated objects. [ENUMERATE]
//!
//! The object that's returned by next() is determined by getobjuuidlist().
//! The default implementation constructs a objuuidlist object. A subclass
//! may instantiate a subclass of objuuidlistObj. [GETOBJECT]
//!
//! This object does not implement locking on the tobjrepo. A subclass
//! may implement nextbatch(), obtain a lock on the given set of objects,
//! then invoke nextbatch() in the superclass while holding the lock.
//! [NEXTBATCH]
//!
//! \note
//! In a multithreaded environment its possible that a given message will not
//! contain the maximum number of uuids in the objuuidlist. This may happen
//! if objects get removed between the time their names are enumerated in the
//! object repository, and their uuids are fetched. In the marginal case, the
//! uuid list may even be empty.

class objuuidenumeratorObj : virtual public x::obj {

protected:
	//! The object repository
	tobjrepo repo;

private:
	tobjrepoObj::obj_iter_t b,	//!< Object iterator
		e;			//!< Ending iterator

public:
	//! Constructor

	objuuidenumeratorObj(const tobjrepo &repoArg);
	~objuuidenumeratorObj() noexcept;

	//! Enumerate the next set of objects.
	x::ptr<objuuidlistObj> next();

private:

	//! Create a new message object

	virtual x::ptr<objuuidlistObj> getobjuuidlist();


protected:
	//! Enumerate the given set of objects.

	//! May be overriden by a subclass, to obtain a lock on the object
	//! names before invoking nextbatch().
	//!
	//! objectnames may be an empty set, if so, the lock may be skipped,
	//! but this nextbatch() still must be invoked.

	virtual x::ptr<objuuidlistObj>
	nextbatch(const std::set<std::string> &objectnames);

};

#endif

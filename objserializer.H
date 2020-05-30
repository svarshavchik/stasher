/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objserializer_H
#define objserializer_H

#include "tobjrepo.H"
#include "newtran.H"

//! Serialize and deserialize an object from a repository

//! This class implements the serialization API for serializing a single object:
//! its name, uuid, and contents, from a repository. The object is specified
//! by name. An object of that name may or may not exist.
//!
//! If the object exists, when the resulting bytestream gets deserialized, the
//! object gets restored, together with its uuid and content, and installed
//! into the repository, bypassing any locking mechanism and any existing
//! content of the object in the destination repository. [DIRECTCOMMIT]
//!
//! If the object does not exist, when the resulting bytestream gets
//! deserialized, the object is removed from the destination repository if it
//! already exists. [DIRECTREMOVE]
//!
//! The constructor takes a reference to a repository. When serializing,
//! a second argument to the constructor specifies the object name.
//! When deserializing, the second argument is ignored and may be omitted.
//!
//! When deserializing, the repository reference passed to the constructor may
//! be a null reference. If so, the deserialized object gets quietly
//! discarded. [DISCARD]
//!
//! The serializer also owns an opaque object reference representing some kind
//! of a locking object. This is used when serializing an object from the
//! repository, so that the lock on the object being serialized is held until
//! serialization is complete.

class objserializer {

	//! Repository
	tobjrepoptr repo;

	//! Object being serialized and deserialized
	std::string objname;

	//! Peer the object is being deserialized from
	std::string peername;

	//! A lock on the object being serialized
	x::ptr<x::obj> lock;

	//! Helper class for specializing the serialization or the deserialization path
	template<bool value> class serialize_impl;

public:

	//! Constructor for deserializing into a repository

	objserializer(//! Transaction repository with the object to deserialize
		      const tobjrepoptr &repoArg,

		      //! Peer from which the transaction is getting deserialized
		      const std::string &peernameArg
		      )
		: repo(repoArg), peername(peernameArg)
	{
	}

	//! Constructor for serializing an object in a repository

	objserializer(//! Transaction repository with the object to serialize
		      const tobjrepoptr &repoArg,

		      //! When serializing, specify the object name to serialize.
		      const std::string &objnameArg,

		      //! Some kind of a lock
		      const x::ptr<x::obj> &lockArg)
		: repo(repoArg), objname(objnameArg), lock(lockArg)
	{
	}

	//! Serialization function
	template<typename ptr_type, typename iter_type>
	static void serialize(ptr_type ptr, iter_type &iter)
	{
		serialize_impl<iter_type::serializing>
			::serialize(ptr->repo,
				    ptr->peername,
				    ptr->objname,
				    iter);
	}

	//! Return the object's name (for debugging/logging purposes)

	const std::string &getName() const
	{
		return objname;
	}
};

//! Specialization for a serializing iterator.

template<> class objserializer::serialize_impl<true> {

public:

	//! Serialize the object.

	template<typename iter_type>
	static void serialize(const tobjrepoptr &repo,
			      const std::string &peername,
			      const std::string &objname,
			      iter_type &iter)
	{
		std::set<std::string> s;

		s.insert(objname);

		objrepoObj::values_t v;
		std::set<std::string> dummy;

		repo->values(s, true, v, dummy);

		// Retrieve the object's uuid and contents.

		objrepoObj::values_t::iterator b(v.begin());

		iter(objname);

		if (b != v.end()) // Object exists
		{
			bool flag=true;
			iter(flag);
			iter(b->second.first);
			iter(*b->second.second);
		}
		else
		{
			bool flag=false; // Object does not exist
			iter(flag);
		}
	}
};

//! Specialization for a deserializing iterator.

template<> class objserializer::serialize_impl<false> {

public:

	//! Deserialize the object.

	template<typename iter_type>
	static void serialize(const tobjrepoptr &repo,
			      const std::string &peername,
			      std::string &objname,
			      iter_type &iter)
	{
		iter(objname);

		bool flag;

		iter(flag);

		if (repo.null())
		{
			// Repository not given, throw this away.

			if (flag)
			{
				x::uuid uuid;

				iter(uuid);

				x::fd fd(x::fd::base::open("/dev/null",
							   O_WRONLY));

				iter(*fd);
			}
			return;
		}

		newtran t(repo->newtransaction());

		t->getOptions()[tobjrepoObj::node_opt]=peername;

		x::uuid uuid;

		if (flag) // Install the object
		{
			iter(uuid);

			x::fd fd(t->newobj(objname));

			repo->tmp_set_uuid(fd, uuid);

			iter(*fd);

			fd->close();
		}
		else	// Remove the object
		{
			t->delobj(objname, uuid);
		}

		x::uuid tuuid=t->finalize();

		repo->commit_nolock(tuuid);
		repo->cancel(tuuid);
	}
};


#endif

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objrepo_dirmsgreply_H
#define objrepo_dirmsgreply_H

#include <x/exception.H>
#include <x/uuid.H>
#include <stasher/namespace.H>
#include <stasher/results.auto.H>

#include <set>

STASHER_NAMESPACE_START

//! The results of a directory request

class dirmsgreply {

public:

	//! Original request uuid
	x::uuid requuid;

	//! Reply message formed here
	x::ptr<getdirresultsObj> msg;

	//! Helper class used when serializing this object on the server side

	class getchunk : virtual public x::obj {

	public:
		//! Status code
		req_stat_t status;

		getchunk() {}
		~getchunk() noexcept {}

		//! Get the next chunk of object names

		virtual bool nextchunk(std::set<std::string> &)
=0;
	};

	//! Serializer helper class instance

	x::ptr<getchunk> serialize_chunk;

	//! Default constructor

	dirmsgreply();

	//! Constructor

	dirmsgreply(const x::uuid &requuidArg);


	//! Destructor

	~dirmsgreply() noexcept;

	template<bool> class serialize_impl;

	//! Serialization function

	template<typename iter_type>
	void serialize(iter_type &iter)
	{
		serialize_impl<iter_type::serializing>::serialize(iter, *this);
	}

	//! Serializing

	template<typename iter_type>
	void impl_serialize(iter_type &iter)
	{
		bool more;

		iter(requuid);

		do
		{
			std::set<std::string> objects;

			more=serialize_chunk->nextchunk(objects);

			iter(objects);
			iter(more);
		} while (more);

		iter(serialize_chunk->status);
	}

	//! Deserializing

	template<typename iter_type>
	void impl_deserialize(iter_type &iter)
	{
		std::set<std::string> temp_objects;

		bool more;

		iter(requuid);

		do
		{
			temp_objects.clear();
			iter(temp_objects);

			msg->objects.insert(temp_objects.begin(),
				       temp_objects.end());
			iter(more);
		} while (more);

		iter(msg->status);
	}
};

//! Stub hook for the serialization path
template<>
class dirmsgreply::serialize_impl<true> {

public:

	//! Invoke serialize_impl() when serialize() gets called with a serialization iterator

	template<typename iter_type>
	static void serialize(iter_type &iter,
			      dirmsgreply &msg)
	{
		msg.impl_serialize(iter);
	}
};

//! Stub hook for the deserialization path
template<>
class dirmsgreply::serialize_impl<false> {

public:

	//! Invoke deserialize_impl() when serialize() gets called with a deserialization iterator

	template<typename iter_type>
	static void serialize(iter_type &iter,
			      dirmsgreply &msg)
	{
		msg.impl_deserialize(iter);
	}
};

STASHER_NAMESPACE_END

#endif

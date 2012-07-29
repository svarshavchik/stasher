/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef transerializer_H
#define transerializer_H

#include "tran.H"
#include "newtran.H"
#include "tobjrepo.H"
#include "spacemonitor.H"
#include "writtenobjbase.H"

#include <x/fd.H>

//! Serialize a pending transaction in a repository

//! Serializing or deserializing this object serializes or deserializes a
//! transaction in a repository.

class transerializer {

	//! UNDEFINED

	transerializer(const transerializer &);

	//! UNDEFINED

	transerializer &operator=(const transerializer &);

	//! The repository
	tobjrepo repo;

public:
	//! Transaction UUID

	//! When serializing, this is initialized by the constructor.
	//! When deserializing, this is not initialized, and after
	//! deserialization contains the transaction's UUID.

	x::uuid uuid;

	//! Transaction object sizes

	//! When serializing, this gets populated after constructing this
	//! object.
	//!
	//! When deserializing, this gets deserialized from the bytestream.

	newtranObj::objsizes_t objsizes;

	//! Flag: deserialization failed due to out of space

	bool outofspace;

	//! Deserialized transaction

	//! This is initialized when deserializing only. The transactionis NOT
	//! finalized.

	newtranptr tran;

	//! Space monitoring object, used when deserializing

	spacemonitorptr df;

	//! Constructor for deserialization

	transerializer(//! The repository
		       const tobjrepo &repoArg,

		       //! Space monitoring object
		       const spacemonitor &dfArg)

		: repo(repoArg), outofspace(false), df(dfArg)
	{
	}

	//! In order to construct writtenObj efficiently, the constructor must have one argument.

	class ser {

	public:
		//! Repository argument
		const tobjrepo *repo;

		//! UUID argument
		const x::uuid *uuid;

		//! Constructor
		ser(//! The repository
		       const tobjrepo *repoArg,

		       //! Specify this parameter to serialize a transaction.
		       //! Leave it out in order to initialize a transerializer
		       //! for deserializing the transaction
		       const x::uuid *uuidArg)
			: repo(repoArg), uuid(uuidArg)
		{
		}

		//! Destructor
		~ser() noexcept
		{
		}
	};

	//! Constructor when serializing a transaction

	transerializer(//! Serialization arguments
		       const ser &serArg)
		: repo(*serArg.repo), uuid(*serArg.uuid),
		  outofspace(false)
	{
	}

	//! Destructor
	~transerializer() noexcept
	{
	}

	template<bool direction> class impl;

	template<bool direction> friend class impl;

	//! Serialization function
	template<typename iter_type> void serialize(iter_type &iter)

	{
		impl<iter_type::serializing>::serialize(*this, iter);
	}
};

//! Serialization implementation class

template<> class transerializer::impl<true> {

public:

	//! Serialize a transaction

	template<typename iter_type>
	static void serialize(transerializer &me,
			      iter_type &iter)

	{
		tranmeta meta;
		size_t n;

		// Populate the size of each object, in its metadata.
		{
			tobjrepoObj::uuidlock ulock(&*me.repo, me.uuid);

			me.repo->parse(meta, me.repo
				       ->open_tran_uuid_locked(me.uuid));

			n=meta.objects.size();

			for (size_t i=0; i < n; ++i)
			{
				if (!meta.objects[i].has_new_value())
					continue;

				meta.objects[i].size=me.repo->tmp_reopen(me.uuid, i)
					->stat()->st_size;
			}
		}

		iter(me.uuid);
		iter(meta.opts);
		iter(meta.objects);

		for (size_t i=0; i < n; ++i)
		{
			if (meta.objects[i].has_new_value())
			{
				x::fd fd(me.repo->tmp_reopen(me.uuid, i));

				iter(fd);
			}
		}
	}
};

//! Deserialization implementation class

template<> class transerializer::impl<false> {

public:

	//! Deserialize a transaction

	template<typename iter_type>
	static void serialize(transerializer &me,
			      iter_type &iter)

	{
		iter(me.uuid);

		me.tran=newtran(new newtranObj(me.repo, me.uuid));

		iter(me.tran->meta.opts);
		iter(me.tran->meta.objects);

		size_t n=me.tran->meta.objects.size();

		// Calculate space needed

		long alloc_units=0;
		long alloc_inodes=0;

		for (size_t i=0; i < n; ++i)
		{
			if (me.tran->meta.objects[i].has_new_value())
			{
				alloc_units += me.df->
					calculate_alloc(me.tran->meta
							.objects[i].size);
				alloc_inodes +=
					spacemonitorObj::calculate_inode
					(me.tran->meta.objects[i].name);
			}
		}

		spacemonitorObj::reservation
			recv=me.df->reservespace_alloc(alloc_units,
						       alloc_inodes);

		me.outofspace=me.df->outofspace();

		for (size_t i=0; i < n; ++i)
		{
			if (me.tran->meta.objects[i].has_new_value())
			{
				x::fd fd=me.outofspace
					? x::fd::base::open("/dev/null",
							    O_WRONLY)
					: me.repo->tmp_create(me.uuid, i);

				iter(fd);
			}
		}

		if (!me.outofspace)
		{
			recv->commit();
			me.df->reservespace_alloc(1, 1)->commit();
			// Account for the metadata
		}
	}
};

#endif

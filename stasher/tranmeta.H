/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef tranmeta_H
#define tranmeta_H

#include <vector>
#include <map>
#include <x/fd.H>
#include <x/uuid.H>

//! Transaction metadata

//! The metadata consists of
//!
//! - arbitrary key/value pairs
//!
//! - zero or more object change record
//!
//! An object change record consists of:
//!
//! - object name
//!
//! - optional existing serial value (a unique ID)
//!
//! - deletion flag
//!
//! The object name is always defined in every object change record. New object
//! records do not have a serial value defined, and the deletion flag is false.
//! An update object change record defines the serial value, and the deletion
//! flag is false. A delete object change record defines the serial value, and
//! the deletion flag is true.
//!
//! This object is serializable
//!
//! \par Class members
//!
//! - \c opts - generic key/value attributes
//!
//! - \c objects - a vector of \c objinfo structures
//!
//! \par Populating object change records
//!
//! newobj() and updobj() append a new object or an update object change
//! record, and return the index of the record in the \c objects vector.
//! delobj() appends a delete object change record.

class tranmeta {

public:
	//! An object change record

	class objinfo {

	public:
		//! Type of a change
		typedef enum {
			new_t,
			upd_t,
			del_t
		} change_t;

		change_t change;	//!< Change type
		std::string name;	//!< Object name
		x::uuid serial;		//!< Existing object's serial number

		off_t size;	//!< Object's size, populated by transerializer

		//! Constructor: new object
		objinfo(const std::string &nameArg) noexcept : change(new_t),
							      name(nameArg),
							      size(0)
		{
		}

		//! Constructor: udpate or delete an existing object
		objinfo(const std::string &nameArg,
			const x::uuid &serialArg,
			bool deleteflagArg) noexcept : change(deleteflagArg ?
							     del_t:upd_t),
						      name(nameArg),
						      serial(serialArg),
						      size(0)
		{
		}

		objinfo() noexcept
		{
		}

		~objinfo() noexcept
		{
		}

		//! Serialization function
		template<typename iter_type> void serialize(iter_type &i)

		{
			uint8_t change_ut;

			if (i.serializing)
				change_ut=(uint8_t)change;

			i(change_ut);

			if (!i.serializing)
				change=(change_t)change_ut;

			i(name);
			i(size);

			if (has_existing_serial())
				i(serial);
		}

		//! Comparison operator
		bool operator==(const objinfo &o) const noexcept
		{
			return change == o.change && name == o.name &&
				(!has_existing_serial() ||
				 serial == o.serial);
		}

		//! Comparison operator
		bool operator!=(const objinfo &o) const noexcept
		{
			return !operator==(o);
		}

		//! Shortcut - this change record specifies an existing serial value
		bool has_existing_serial() const noexcept
		{
			return change != new_t;
		}

		//! Shortcut - this change record specifies a new value
		bool has_new_value() const noexcept
		{
			return change != del_t;
		}
	};

	tranmeta() noexcept
	{
	}

	~tranmeta() noexcept
	{
	}

	//! Generic options

	std::map<std::string, std::string> opts;

	//! List of objects in this transaction
	std::vector<objinfo> objects;

	//! New object change record

	size_t newobj(const std::string &objname)
	{
		size_t n=objects.size();

		objects.push_back(objinfo(objname));
		return n;
	}

	//! Update object change record
	size_t updobj(const std::string &objname,
		      const x::uuid &sv)
	{
		size_t n=objects.size();

		objects.push_back(objinfo(objname, sv, false));

		return n;
	}

	//! Delete object change record
	void delobj(const std::string &objname,
		    const x::uuid &sv)
	{
		objects.push_back(objinfo(objname, sv, true));
	}

	//! Deserialized an object change record
	size_t deserobj(const objinfo &obj)
	{
		size_t n=objects.size();

		objects.push_back(obj);
		return n;
	}

	//! Serialization function
	template<typename iter_type> void serialize(iter_type &i)

	{
		i(opts);
		i(objects);
	}

	//! Comparison operator
	bool operator!=(const tranmeta &o) const noexcept
	{
		return !operator==(o);
	}

	//! Comparison operator
	bool operator==(const tranmeta &o) const noexcept
	{
		return opts == o.opts && objects == o.objects;
	}
};

#endif

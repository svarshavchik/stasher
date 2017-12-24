/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_puttransaction_H
#define stasher_puttransaction_H

#include <x/exception.H>
#include <x/fd.H>
#include <x/uuid.H>
#include <string>
#include <stasher/namespace.H>
#include <stasher/serobjname.H>

class localconnectionObj;

STASHER_NAMESPACE_START

//! A transaction that will be submitted to a repository

class puttransactionObj : virtual public x::obj {

public:

	//! The contents of an object come from here

	class objcontentsObj : virtual public x::obj {

	public:
		//! It's in here
		x::fdptr contents_filedesc;

		//! If not in contents_filedesc, it's in here

		std::string contents;

		//! Constructor - contents in a file descriptor

		objcontentsObj(const x::fd &filedesc);

		//! Constructor -- contents in a string
		objcontentsObj(const std::string &contentsArg);
		
		//! Destructor
		~objcontentsObj();
	};

	//! The contents of a new or an updated objects

	//! The contents may come from a file, or a std::string

	class objcontents : public x::ptr<objcontentsObj> {

	public:
		//! Constructor
		objcontents();

		//! Destructor
		~objcontents();

		//! Constructor - contents in a file descriptor
		objcontents(const x::fd &filedesc);

		//! Constructor -- contents in a string
		objcontents(const std::string &str);

		using x::ptr<objcontentsObj>::operator->;
		using x::ptr<objcontentsObj>::operator*;
	};

	//! Information about an object being updated

	class updinfo {

	public:
		//! Object's name
		serobjname name;

		//! This is a new object
		bool newflag;

		//! uuid of an existing object
		x::uuid uuid;

		//! This object is being deleted
		bool delflag;

		//! The contents of this object
		objcontents contents;

		//! Size of the object's contents

		off_t contents_size;

		//! A new object

		updinfo(//! New object name
			const std::string &nameArg,

			//! New object's contents
			const objcontents &contentsArg);

		//! Update an object

		updinfo(//! Existing object
			const std::string &nameArg,

			//! Existing object's uuid
			const x::uuid &uuidArg,

			//! Updated contents of this object
			const objcontents &contentsArg);

		//! Delete an object
		updinfo(//! Existing object
			const std::string &nameArg,

			//! Existing object's uuid
			const x::uuid &uuidArg);

		//! Default constructor, used when deserializing
		updinfo();

		//! Destructor
		~updinfo();

		//! The serialization function

		//! Iterate over everything except the contents
		//!
		template<typename iter_type>
		void serialize(iter_type &iter)
		{
			name.serialize(iter);
			iter(contents_size);
			iter(newflag);
			iter(delflag);
			if (!newflag)
				iter(uuid);
		}
	private:

		//! Set the contents' size

		void setsize();
	};

	//! Objects to update
	std::vector<updinfo> objects;

	//! Admin connection wants to update the transaction directly in the local repo
	bool admin;

	//! Compute aggregate total object size
	off_t totsize() const;

	//! Whether this is an empty transaction
	bool empty();


	puttransactionObj();
	~puttransactionObj();

	//! Maximum number of objects that can be deserialized

	size_t maxobjects;
	//! Serialization function

	template<typename iter_type>
	void serialize(iter_type &iter)
	{
		iter(objects, maxobjects);
		iter(admin);
	}

	//! The content of the object is given in a string

	class content_str {
	public:
		//! The string
		std::string str;

		//! Maximum length of a deserialized string
		size_t chunksize;

		//! Constructor for deserialization (regression tests)
		content_str(size_t chunksizeArg)
			: chunksize(chunksizeArg)
		{
		}

		//! Constructor for deserialization
		content_str(const localconnectionObj &conn);

		//! Constructor for serialization
		content_str(const std::string &strArg)
			: str(strArg)
		{
		}

		//! Destructor
		~content_str()
		{
		}

		//! Serialization function

		template<typename iter_type>
		void serialize(iter_type &iter)
		{
			iter(str, chunksize);
		}
	};

	//! Add a new object
	template<typename value_type>
	void newobj(//! New object name
		    const std::string &objname,

		    //! New object's contents
		    const value_type &contents)
	{
		newobj_impl(objname, objcontents(contents));
	}

	//! Update the contents of an existing object

	template<typename value_type>
	void updobj(//! Existing object
		    const std::string &objname,

		    //! Existing object's uuid
		    const x::uuid &uuid,

		    //! Updated contents of this object
		    const value_type &contents)
	{
		updobj_impl(objname, uuid, objcontents(contents));
	}

	//! Delete the object

	void delobj(//! Existing object
		    const std::string &objname,

		    //! Existing object's uuid
		    const x::uuid &uuid);

private:
	//! Add a new object

	void newobj_impl(//! New object name
		    const std::string &objname,

		    //! New object's contents
		    const objcontents &contents);

	//! Update the contents of an existing object

	void updobj_impl(//! Existing object
			 const std::string &objname,

			 //! Existing object's uuid
			 const x::uuid &uuid,

			 //! Updated contents of this object
			 const objcontents &contents);
};

STASHER_NAMESPACE_END

#endif

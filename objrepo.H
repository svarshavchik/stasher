/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objrepo_H
#define objrepo_H

#include <x/fd.H>
#include <x/logger.H>
#include <x/fileattr.H>
#include <x/weaklist.H>
#include <x/uuid.H>

#include <sys/types.h>
#include <sys/stat.h>
#include <shared_mutex>

#include "stasher/namespace.H"
#include "objrepofwd.H"
#include <filesystem>
#include <tuple>

STASHER_NAMESPACE_START
class userput;
STASHER_NAMESPACE_END

//! Object repository

//! Where objects live. An "object" is a file that's identified by a name.
//! The name cannot be empty. Object names use "/" for a hierarchy separator.
//! "/" cannot be the first or the last character in the name, and the name
//! cannot contain consecutive "/" characters. The maximum length
//! of the object name is 512 bytes.
//!
//! Objects are installed and removed from the object repository atomatically.
//! Installing an object involves creating a temporary object file, this
//! process returns a file descriptor where the object's value is written to.
//! After the file descriptor is closed, the object is ready to be atomatically
//! installed.
//!
//! \par One instance per repository directory
//!
//! Instantiating an objrepoObj creates the named directory if it does not
//! already exist. [CREATE]
//!
//! Only one objrepoObj instance may be bound to a directory. The constructor
//! throws an exception if another process has bound an objrepoObj to a
//! directory. The locking using fcntl(), so instantiating another objrepoObj
//! bound to the same directory in the same process does not throw an exception,
//! but the results are undefined. [LOCK]
//!
//! \par Temporary files
//!
//! The contents of an object are first saved into a temporary file.
//!
//! A temporary filename must be non-empty, cannot begin with the "."
//! character and cannot contain "/" characters. Passing an invalid filename
//! to these methods throws an exception. [TMPVALID]
//!
//! tmp_open() opens a temporary file.
//!
//! Existing temporary filenames in a repository directory are iterated by
//! invoking tmp_iter(), which returns a two tmp_iter_t objects,
//! which are iterator types. tmp_iter() returns the beginning and the
//! ending iterator. // [TMPITER]
//!
//! tmp_get() returns an x::fileattr object, that can be used to retrieve
//! attributes of a temporary file. [TMPGET]
//!
//! tmp_rename() renames a temporary file. [TMPRENAME]
//!
//! tmp_link() links a temporary file to another one. [TMPLINK]
//!
//! \par Objects
//!
//! obj_install() installs a temporary file as an object. [OBJINSTALL]
//!
//! obj_remove() removes an object from the repository. [OBJREMOVE]
//!
//! obj_exists() checks if an object exists in the repository. [OBJEXISTS]
//!
//! obj_get() returns an x::fileattr object that can be used to retrieve
//! attributes of an object in the repository. [OBJGET]
//!
//! obj_open() returns an opened file descriptor of an existing object.
//! A NULL file descriptor gets returned if the object does not exist. [OPEN]
//!
//! Names of all existing objects in a repository directory are iterated by
//! invoking obj_begin() and obj_end(), which return an obj_iter_t.
//! *obj_iter_t is convertible to a std::string. [OBJITER]
//!
//! Names of existing objects in a single repository directory are iterated by
//! inboking dir_begin() and dir_end(), which return a dir_iter_t.
//! *dir_iter_t is convertible to a std::string. A trailing / in the string
//! designates a subdirectory. [OBJDIRITER]
//!
//!
//! Invoke values() to retrieve objects from the repository.
//!
//! \par Validation
//!
//! Objects are mapped directory onto the underlying filesystem (with some
//! filename munging). Subdirectories are automatically created and deleted,
//! as objects are installed and removed. [SUBDIRS]
//!
//! When a repository object gets instatiated, it validates the contents of
//! the repository directory by walking it, removing any empty subdirectories
//! or files that do not represent valid object names; also the temporary
//! directory is purged of any files that do not meet the temporary filename
//! naming convetions. [VALID]
//!
//! \par Notifiers [NOTIFIERS]
//!
//! A notification mechanisms allows notifications of changes to the repository.
//! Derive from objrepoObj::notifierObj, and define installed() and
//! removed(), which will get invoked immediately after an object has been
//! installed or removed from the object repository, just before
//! obj_install() and obj_remove() returns.
//!
//! installNotifier() installs a notification object, whose installed() and
//! removed() methods will get invoked until the callback object gets
//! destroyed.
//!
//! obj_install and obj_remove take an opaque object reference, that represents
//! any current lock being held by the higher level class. The opaque object
//! is passed through to the notifiers, allowing them to keep the lock
//! while the changed objects are being processed, in some way.

class objrepoObj : virtual public x::obj {

public:

	//! Maximum object name size

	static const size_t maxnamesize=512;

	//! Extended attribute that tobjrepoObj uses to store an object's serial value.

	//! objrepoObj needs to know this in order to clean up the object repo.

	static const char xattrserial[];

	//! Construct and bind to an object repository directory
	objrepoObj(const std::string &directoryArg);

	//! Destructor
	~objrepoObj();

	//! Open a temporary file.

	x::fd tmp_open(//! Temporary file name
		       const std::string &tmpfilename,

		       //! File open flags, same as for %open(2).

		       int flags,

		       //! File creation mode, if O_CREAT

		       mode_t openmode=0666);

	//! Return an attribute reference for a temporary file.

	x::fileattr tmp_get(//! Temporary file name
			    const std::string &tmpfilename);

	//! Check whether a temporary file exists

	bool tmp_exists(//! Temporary file name
			const std::string &tmpfilename);

	//! Rename a temporary filename

	void tmp_rename(//! Old name
			const std::string &oldname,

			//! New name
			const std::string &newname);

	//! Hardlink a temporary filename

	void tmp_link(//! Old name
		      const std::string &oldname,

		      //! New name
		      const std::string &newname);

	//! Remove a temporary filename

	void tmp_remove(const std::string &tmpfilename);

	//! Iterators over temporary files in the object repository directory

	typedef std::filesystem::directory_iterator tmp_iter_t;

	//! Return a beginning and ending iterator

	std::tuple<tmp_iter_t, tmp_iter_t> tmp_iter();

	// ---

	//! Install a new object

	void obj_install(//! Temporary filename
			 const std::string &tmpfilename,

			 //! Object name
			 const std::string &objname,

			 //! An opaque object lock being held at a higher level,
			 //! passed through to any notifiers

			 const x::ptr<x::obj> &lock
			 );

	//! Remove an existing object

	void obj_remove(//! Object name
			const std::string &objname,

			 //! An opaque object lock being held at a higher level,
			 //! passed through to any notifiers

			 const x::ptr<x::obj> &lock);

	//! Check if an object exists

	bool obj_exists(//! Object name
			const std::string &objname);

	//! Return an attribute reference for an object.

	x::fileattr obj_get(//! Object name
			    const std::string &objname);

	//! Open an object

	x::fdptr obj_open(//! Object name
			  const std::string &objname);

	//! An iterator over objects in the repository

	class obj_iter_t {

		//! The repository
		objrepoptr repo;

		//! Beginning iterator over the entire directory hierarchy
		std::filesystem::recursive_directory_iterator b;

		//! Ending iterator over the entire directory hierarchy
		std::filesystem::recursive_directory_iterator e;

		//! Name of the current object the iterator is referencing
		std::string n;

		//! Translate the filename to an object name

		void n_init();

		//! Constructor

		obj_iter_t(
			const objrepo &repo,
			const std::filesystem::recursive_directory_iterator &b,
			const std::filesystem::recursive_directory_iterator &e
		);

	public:
		typedef std::input_iterator_tag iterator_category;
		typedef std::string value_type;
		typedef void difference_type;
		typedef void pointer;
		typedef void iterator;

		friend class objrepoObj;

		//! Ending iterator constructor
		obj_iter_t();

		//! Comparison operator
		bool operator==(const obj_iter_t &o) noexcept
		{
			return repo.null() && o.repo.null();
		}

		//! Comparison operator
		bool operator!=(const obj_iter_t &o) noexcept
		{
			return !operator==(o);
		}

		//! Dereference operator
		const std::string &operator*() noexcept
		{
			return n;
		}

		//! Pre increment operator
		obj_iter_t &operator++();

		//! Post increment operator
		obj_iter_t operator++(int)
		{
			obj_iter_t cpy;

			cpy.n=n;

			operator++();
			return cpy;
		}
	};

	//! An iterator over objects in the same directory in the repository

	//! The iterated over name ends with a trailing / for a subdirectory

	class dir_iter_t {

		//! The repository
		objrepoptr repo;

		//! Beginning iterator over the entire directory hierarchy
		std::filesystem::directory_iterator b;

		//! Ending iterator over the entire directory hierarchy
		std::filesystem::directory_iterator e;

		//! Name of the current object the iterator is referencing
		std::string n;

		//! Translate the filename to an object name

		void n_init();

		//! Constructor

		dir_iter_t(const objrepo &repo,
			   const std::filesystem::directory_iterator &b,
			   const std::filesystem::directory_iterator &e);


	public:
		typedef std::input_iterator_tag iterator_category;
		typedef std::string value_type;
		typedef void difference_type;
		typedef void pointer;
		typedef void iterator;

		friend class objrepoObj;

		//! Ending iterator constructor
		dir_iter_t();

		//! Comparison operator
		bool operator==(const dir_iter_t &o) noexcept
		{
			return repo.null() && o.repo.null();
		}

		//! Comparison operator
		bool operator!=(const dir_iter_t &o) noexcept
		{
			return !operator==(o);
		}

		//! Dereference operator
		const std::string &operator*() noexcept
		{
			return n;
		}

		//! Dereference operator
		const std::string *operator->() noexcept
		{
			return &n;
		}

		//! Pre increment operator
		dir_iter_t &operator++();

		//! Post increment operator
		dir_iter_t operator++(int)
		{
			dir_iter_t cpy;

			cpy.n=n;

			operator++();
			return cpy;
		}
	};

	friend class obj_iter_t;
	friend class dir_iter_t;
	friend class STASHER_NAMESPACE::userput;

	//! Return a beginning iterator over names of objects in the repository.
	obj_iter_t obj_begin(const std::string &hier="");

	//! Return an ending iterator over names of objects in the repository.
	obj_iter_t obj_end();

	//! Return a beginning iterator over names of objects in a directory in the repository
	dir_iter_t dir_begin(const std::string &hier="");

	//! Return an ending iterator over names of objects in a directory in the repository.
	dir_iter_t dir_end();

	//! Return value when retrieving objects

	//! values() populates this map, keyed by object name, with the
	//! value being the object's serial number and, optionally, an open
	//! file descriptor giving the object's value.

	typedef std::map<std::string, std::pair<x::uuid, x::fdptr> > values_t;

	//! Retrieve objects
	void values(//! Names of objects
		    const std::set<std::string> &objects,

		    //! If true, populate the file descriptor part of the map
		    //! value, if false leave the file descriptor unpopulated.

		    bool openflag,

		    //! Values of objects. Objects that do not exist will not
		    //! have a key for their name, only keys of objects that
		    //! exist will have their keys set.

		    values_t &retvalues,

		    //! Objects that were not found, and not placed in
		    //! retvalues, get placed into this set.

		    std::set<std::string> &notfound);

	//! A notification callback object base

	class notifierObj : virtual public x::obj {

	public:
		//! Constructor
		notifierObj();

		//! Destructor
		~notifierObj();

		//! An object was just installed
		virtual void installed(const std::string &objname,
				       const x::ptr<x::obj> &lock)=0;

		//! An object was just removed
		virtual void removed(const std::string &objname,
				       const x::ptr<x::obj> &lock)=0;
	};

	//! A reference to a notifier object.

	typedef x::ptr<notifierObj> notifier;

	//! Register a notifier object

	//! The notifier object gets invoked after any change to the repository.
	//!

	void installNotifier(const notifier &n);

private:
	//! The logger class

	LOG_CLASS_SCOPE;

	//! Compute top directory for object iterator

	std::string topdir(const std::string &hier);

public:
	//! Repository directory
	const std::string directory;
private:
	//! The lock file
	x::fd lock;

	//! The weak list of registered notifiers

	typedef x::weaklist<notifierObj> notifier_list_t;

	//! Registered notifiers

	notifier_list_t notifiers;

	//! Create a full pathname to a temporary file

	std::string valid_tmp_name(const std::string &s);

protected:
	//! Compute size of buffer to hold the full pathname to an object file
	size_t obj_name_len(const std::string &s);

	//! Create a full pathname to an object file
	void obj_name_create(const std::string &s, char *p);

private:
	//! Take a full pathname and decode the object name from it
	std::string fullpath_to_objname(const std::string &s) noexcept;

	void invalid_object_name()
		__attribute__((noreturn));
protected:
	//! tmp_rename and tmp_remove() acquire a read lock on tmp_writelock

	//! Acquire a write lock to be able to enumerate tmp files while
	//! blocking their removal.

	std::shared_mutex tmp_writelock;
};

#endif

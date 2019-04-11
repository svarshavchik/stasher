/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef nsview_H
#define nsview_H

#include <string>
#include <map>

#include <x/exception.H>
#include <x/logger.H>
#include <x/obj.H>
#include <x/ref.H>

//! A client connection's view on the object namespace

//! The top level hierarchy of a client's object namespace is mapped through
//! a view. This view combines the specified read/write and read/only
//! namespace mappings that are selected based on the client's credentials.

class nsviewObj : virtual public x::obj {

	//! The logger

	LOG_CLASS_SCOPE;

	//! The type of the view map.

	//! Key is the top level namespace, value is that namespace's
	//! hierarchy root, and whether the client has write privileges.

	typedef std::map<std::string, std::pair<std::string, bool> > view_t;

	//! The view map.
	view_t view;

	//! Merge a read/write or a read-only mapping, called by the constructor

	void merge(const std::map<std::string, std::string> &nsmap,
		   bool rw);

	//! Perform mapping of client object name according to the namespace mapping

	//! \return \c true if success, \c false if "write" is set and the
	//! namespace mapping is read-only.

	bool fromclient(//! Object name, modified if mapped via a namespace
			std::string &name,

			//! When the object is for the purpose of being updated
			bool write)
		const;

public:
	//! Constructor
	nsviewObj(//! Read only namespace mapping for the client
		  const std::map<std::string, std::string> &ro,

		  //! Read/write namespace mapping for the client
		  const std::map<std::string, std::string> &rw,

		  //! Default client r/w root, unless overriden by mappings
		  const char *root_view);


	//! Default constructor for admin connections, global r/w root
	nsviewObj();
	~nsviewObj() {}

	//! Map an object name from client view, for reading.

	//! \return false if the object name cannot be read, true otherwise.
	//!
	bool fromclient_read(std::string &objname)
		const
	{
		return fromclient(objname, false);
	}

	//! Map an object name from client view, for writing.

	//! \return false if the object name cannot be changed, true otherwise.
	//!
	bool fromclient_write(std::string &objname)
		const
	{
		return fromclient(objname, true);
	}

	//! Map a directory name, for retrieving its contents
	bool fromclient_dir(std::string &dirname);

	//! Retrieve the entire view definition
	const view_t &getView() const noexcept { return view; }

	//! Display the contents of this view

	void to_string(std::ostream &o) const;
};

//! A reference to a view

typedef x::ref<nsviewObj> nsview;

#endif

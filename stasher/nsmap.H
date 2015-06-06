/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef nsmap_H
#define nsmap_H

#include <string>
#include <map>
#include <set>
#include <vector>

#include <x/exception.H>
#include <x/filestat.H>
#include <x/logger.H>

#include "objrepo_config.h"

//! Object namespace map

struct nsmap {

	//! The logger
	LOG_CLASS_SCOPE;

public:
	std::string label;	//!< This mapping's label
	std::string host;	//!< Mapping for any client on this host
	std::string path;	//!< Mapping for this executable binary
	std::string username;	//!< Mapping for this username
	std::string groupname;	//!< Mapping for this groupname

	//! The namespace map

	//! Key is a top-level object hierarchy name. The value is the
	//! "real" top-level object hierarchy, separated by /.
	//!
	//! An empty key specifies the default real top-level hierarchy, if
	//! no match for any explicitly defined mapping. Non-admin connections
	//! have the initial top-level hierarchy set to "sandbox". Admin
	//! connections ignore any top-level hierarchy setting, and force it
	//! to the real top level of the object namespace.
	//!
	//! The actual namespace map is a std::vector of these objects, which
	//! are then processed for each client connection, one record at a
	//! time.
	//!
	//! A value of an empty string drops any mapping for this namespace,
	//! set by any previous record in the std::vector.

	std::map<std::string, std::string> map;

	//! Remove any existing mapping for this hierarchy
	std::set<std::string> unmap;

	//! Serialization function

	template<typename iter_type> void serialize(iter_type &iter)

	{
		iter(label);
		iter(host);
		iter(path);
		iter(username);
		iter(groupname);
		iter(map);
		iter(unmap);
	}

	//! Comparison operator

	bool operator<(const nsmap &o)
	{
		if (label.size() || o.label.size())
			return label < o.label;

		if (host < o.host)
			return true;

		if (o.host < host)
			return false;

		if (path < o.path)
			return true;

		if (o.path < path)
			return false;

		if (username < o.username)
			return true;

		if (o.username < username)
			return false;

		if (groupname < o.groupname)
			return true;

		return false;
	}

	//! Comparison operator

	bool operator==(const nsmap &o)
	{
		return (label.size() || o.label.size()
			? label == o.label
			: (host == o.host &&
			   path == o.path &&
			   username == o.username &&
			   groupname == o.groupname));
	}

	//! Connecting client's credentials

	class clientcred {
	public:
		//! Client's executable path
		std::string path;

		//! Client's username
		std::string username;

		//! Client's groupname
		std::string groupname;

		//! Constructor
		clientcred(//! Client's executable path
			   const std::string &pathArg,
			   //! Client's username
			   const std::string &usernameArg,
			   //! Client's groupname
			   const std::string &groupnameArg);

		//! Convert the credentials to a string.
		operator std::string() const;

		//! Run client's credentials through a map definition, produce a map.

		std::map<std::string, std::string>
		computemappings(//! Defined namespace map
				const std::vector<nsmap> &map,

				//! Client's hostname
				const std::string &hostname)
			const;
	};

	//! Locally-defined application mappings

	//! This map is keyed by dev/ino pair, identifying an installed
	//! app.
	//! The map's value is a pair of the app's read/write namespaces and
	//! the app's read/only namespaces.

	typedef std::map<std::pair<dev_t, ino_t>,
			 std::pair<std::map<std::string, std::string>,
				   std::map<std::string, std::string> > >
	local_map_t;

	//! Create a local map by reading config files in a directory

	static void get_local_map(//! The directory
				  const std::string &dirname,

				  //! The map gets loaded here
				  local_map_t &localmap);

private:
	class pathinfo {
	public:
		std::string path;
		std::pair<dev_t, ino_t> devino;
		bool given;

		pathinfo() : given(false)
		{
		}
	};

	static void parse_local_map(const std::string &configfile,
				    nsmap::local_map_t &localmap);

};

#endif

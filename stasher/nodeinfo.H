/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_nodeinfo_H
#define stasher_nodeinfo_H

#include <string>
#include <map>
#include <vector>
#include <x/exception.H>
#include <x/chrcasecmp.H>

#include <stasher/namespace.H>

STASHER_NAMESPACE_START

class nodeinfo;

//! All nodes in the cluster

typedef std::map<std::string, nodeinfo> nodeinfomap;

//! Information about a node in the object repository cluster

class nodeinfo {

	//! Convenience function: value of a BOOL option

	bool bool_option(const std::string &option_name) const noexcept;

	//! Convenience function: set/unset a BOOL option

	void bool_option(const std::string &option_name,
			 bool option_value) noexcept;

	//! Option name
	static const char nomaster_option[];

	//! Option name
	static const char encryption_option[];

public:

	//! A container for generic option=value pairs.

	//! The option name is case-insensitive.
	//!
	typedef std::multimap<std::string, std::string,
			      x::chrcasecmp::str_less> options_t;

	//! Option name
	static const char ordernum_option[];

	//! Option name
	static const char host_option[];

	//! Generic option=value configuration settings.

	options_t options;

	//! Option: this node does not really want to be the master

	bool nomaster() const noexcept
	{
		return bool_option(nomaster_option);
	}

	//! Option: this node does not really want to be the master

	void nomaster(bool value) noexcept
	{
		bool_option(nomaster_option, value);
	}

	//! Option: use an encrypted connection for connection to this node

	bool useencryption() const noexcept
	{
		return bool_option(encryption_option);
	}

	//! Option: use an encrypted connection for connection to this node

	void useencryption(bool value) noexcept
	{
		bool_option(encryption_option, value);
	}

	//! Option: relative ordering of nodes in a cluster
	int ordernum() const noexcept;

	//! Option: relative ordering of nodes in a cluster
	void ordernum(int n) noexcept;


	nodeinfo() noexcept;
	~nodeinfo();

	//! Comparison function

	bool operator==(const nodeinfo &o) noexcept
	{
		return options == o.options;
	}

	//! Comparison function

	bool operator!=(const nodeinfo &o) noexcept
	{
		return !operator==(o);
	}

	//! Serialization function

	template<typename ptr_type, typename iter_type>
	static void serialize(ptr_type ptr, iter_type &iter)
	{
		iter(ptr->options);
	}

	//! Dump a report

	void to_string(std::ostream &o);


	//! A cluster node property used for ordering

	class order_comp_func;

	//! Retrieve node ordering

	static void loadnodeorder(nodeinfomap &cluster,
				  std::vector<nodeinfomap::iterator> &ordervec)
		noexcept;

	//! Update node ordering

	static void savenodeorder(const std::vector<nodeinfomap::iterator>
				  &ordervec) noexcept;
};

STASHER_NAMESPACE_END

#endif

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "stasher/nodeinfo.H"

#include <x/ctype.H>

STASHER_NAMESPACE_START

const char nodeinfo::nomaster_option[]="NOMASTER";

const char nodeinfo::encryption_option[]="ENCRYPTION";

const char nodeinfo::host_option[]="HOST";

const char nodeinfo::ordernum_option[]="ORDERNUM";

nodeinfo::nodeinfo() noexcept
{
}

nodeinfo::~nodeinfo() noexcept
{
}

bool nodeinfo::bool_option(const std::string &option_name) const noexcept
{
	for (std::pair<options_t::const_iterator,
		       options_t::const_iterator>
		     iter(options.equal_range(option_name));
	     iter.first != iter.second; ++iter.first)
	{
		switch (*iter.first->second.c_str()) {
		case 'n':
		case 'N':
		case '0':
			break;
		default:
			return true;
		}
	}
	return false;
}

void nodeinfo::bool_option(const std::string &option_name,
			   bool option_value) noexcept
{
	std::pair<options_t::iterator,
		  options_t::iterator>
		iter(options.equal_range(option_name));

	options.erase(iter.first, iter.second);

	if (option_value)
		options.insert(std::make_pair(option_name,
					      std::string("1")));
}

int nodeinfo::ordernum() const noexcept
{
	int n=0;

	options_t::const_iterator iter=options.find(ordernum_option);

	if (iter != options.end())
		std::istringstream(iter->second) >> n;

	return n;
}

void nodeinfo::ordernum(int n) noexcept
{
	std::pair<options_t::iterator,
		  options_t::iterator>
		iter(options.equal_range(ordernum_option));

	options.erase(iter.first, iter.second);

	std::ostringstream o;

	o << n;

	options.insert(std::make_pair(ordernum_option, o.str()));
}

void nodeinfo::toString(std::ostream &o)
{
	x::ctype c(x::locale::base::utf8());

	for (auto opt:options)
	{
		if (opt.first == ordernum_option)
			continue;

		o << "    " << c.toupper(opt.first) << "="
		  << opt.second << std::endl;
	}
}


// ----------------------------------------------------------------------------

class LIBCXX_HIDDEN nodeinfo::order_comp_func {

 public:
	typedef std::pair<int, STASHER_NAMESPACE::nodeinfomap
			  ::iterator> iter_t;

	bool operator()(const iter_t &a, const iter_t &b)
	{
		return a.first < b.first;
	}
};

void nodeinfo::loadnodeorder(nodeinfomap &cluster,
			     std::vector<nodeinfomap::iterator> &ordervec)
	noexcept
{
	typedef std::vector<std::pair<int, nodeinfomap::iterator> > ov_t;

	ov_t ov;

	ov.reserve(cluster.size());

	for (nodeinfomap::iterator b=cluster.begin(), e=cluster.end();
	     b != e; ++b)
		ov.push_back(std::make_pair(b->second.ordernum(), b));

	std::sort(ov.begin(), ov.end(), order_comp_func());

	ordervec.clear();
	ordervec.reserve(ov.size());

	for (ov_t::iterator b=ov.begin(), e=ov.end(); b != e; ++b)
		ordervec.push_back(b->second);
}

void nodeinfo::savenodeorder(const std::vector<nodeinfomap::iterator> &ordervec)
	noexcept
{
	size_t n=0;

	for (std::vector<nodeinfomap::iterator>::const_iterator
		     b=ordervec.begin(), e=ordervec.end(); b != e; ++b, ++n)
	{
		(*b)->second.ordernum(n);
	}
}

STASHER_NAMESPACE_END

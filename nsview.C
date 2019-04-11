/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "nsview.H"

LOG_CLASS_INIT(nsviewObj);

void nsviewObj::merge(const std::map<std::string, std::string> &nsmap,
		   bool rw)
{
	for (std::map<std::string, std::string>::const_iterator
		     b=nsmap.begin(),
		     e=nsmap.end(); b != e; ++b)
	{
		view[b->first]=std::make_pair(b->second, rw);
	}
}

nsviewObj::nsviewObj(const std::map<std::string, std::string> &ro,
		     const std::map<std::string, std::string> &rw,
		     const char *root_view)

{
	merge(ro, false);
	merge(rw, true);

	if (view.find("") == view.end())
		view.insert(std::make_pair("", std::make_pair(root_view,
							      true)));
}

nsviewObj::nsviewObj()

{
	view.insert(std::make_pair("", std::make_pair("", true)));
}

void nsviewObj::to_string(std::ostream &o) const
{
	for (view_t::const_iterator b=view.begin(), e=view.end(); b != e; ++b)
	{
		o << "    "
		  << (b->first.size()
		      ? b->first: "(root namespace)")
		  << " => " << b->second.first
		  << (b->second.second ? " (rw)":" (ro)") << std::endl;
	}
}

bool nsviewObj::fromclient(std::string &name, bool write)
	const
{
	size_t tlhier_len, subhier_start;

	if ((tlhier_len=name.find('/')) != std::string::npos)
	{
		subhier_start=tlhier_len+1;
	}
	else
		tlhier_len=subhier_start=0;

	view_t::const_iterator iter;

	if (tlhier_len == 0 || (iter=view.find(name.substr(0, tlhier_len))) ==
	    view.end())
	{
		tlhier_len=subhier_start=0;

		if ((iter=view.find("")) == view.end())
			return false;
	}

	if (write && !iter->second.second)
		return false;

	std::string newtlhier=iter->second.first;

	if (newtlhier.size() > 0 && subhier_start < name.size())
		newtlhier += "/";
	name=newtlhier + name.substr(subhier_start);
	return true;
}

bool nsviewObj::fromclient_dir(std::string &dirname)
{
	if (dirname.size() == 0)
		return fromclient(dirname, false);

	std::string n=dirname + "/.";

	if (!fromclient(n, false))
		return false;

	if (n.size() >= 2) // Should always be the case
		dirname=n.substr(0, n.size()-2);
	return true;
}

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "nsmap.H"
#include <x/fd.H>
#include <x/locale.H>
#include <x/strtok.H>
#include <x/fileattr.H>
#include <sstream>
#include <filesystem>

LOG_CLASS_INIT(nsmap);

nsmap::clientcred::clientcred(const std::string &pathArg,
			      const std::string &usernameArg,
			      const std::string &groupnameArg)

	: path(pathArg), username(usernameArg),
	  groupname(groupnameArg)
{
}

nsmap::clientcred::operator std::string() const
{
	std::ostringstream o;

	o << "client: "
	  << path << " (" << username << ":" << groupname << ")";
	return o.str();
}

std::map<std::string, std::string>
nsmap::clientcred::computemappings(const std::vector<nsmap> &map,
				   const std::string &hostname)
	const
{
	std::map<std::string, std::string> mapping;

	for (std::vector<nsmap>::const_iterator
		     b=map.begin(), e=map.end(); b != e; ++b)
	{
		if (b->host.size() && b->host != hostname)
			continue;

		if (b->path.size() && b->path != path)
			continue;

		if (b->username.size() && b->username != username)
			continue;

		if (b->groupname.size() && b->groupname != groupname)
			continue;

		for (std::map<std::string, std::string>::const_iterator
			     mb=b->map.begin(),
			     me=b->map.end(); mb != me; ++mb)
		{
			mapping[mb->first]=mb->second;
		}

		for (std::set<std::string>::const_iterator
			     ub=b->unmap.begin(),
			     ue=b->unmap.end(); ub != ue; ++ub)
		{
			mapping.erase(*ub);
		}
	}

	return mapping;
}

void nsmap::get_local_map(const std::string &dirname,
			  local_map_t &localmap)
{
	std::error_code ec;

	for (auto &direntry:std::filesystem::directory_iterator{
			dirname, {}, ec})
	{
		if (!direntry.is_regular_file())
			continue;

		std::string s=direntry.path().filename();

		if (s.find('#') != std::string::npos ||
		    s.find('~') != std::string::npos ||
		    *s.c_str() == '.')
			continue;

		try {
			parse_local_map(direntry.path(), localmap);
		} catch (const x::exception &e)
		{
			LOG_ERROR(direntry.path() << ": "
				  << e);
		}
	}
}

void nsmap::parse_local_map(const std::string &configfile,
			    nsmap::local_map_t &localmap)
{
	pathinfo pi;

	x::istream i= x::fd::base::open(configfile, O_RDONLY)->getistream();

	auto l=x::locale::base::environment();

	do
	{
		std::string s;

		std::getline(*i, s);

		std::list<std::string> words;

		x::strtok_str(s, " \t\r\n", '"', words);

		if (words.empty())
			continue;

		std::string cmd=l->toupper(words.front());

		words.pop_front();

		if (!pi.given && cmd != "PATH")
		{
			throw EXCEPTION("The first command must be \"PATH\"");
		}

		if (cmd == "PATH" && !words.empty())
		{
			pi.path=words.front();
			pi.given=true;
			if (access(pi.path.c_str(), R_OK))
			{
				LOG_WARNING(pi.path << " does not exist");
				pi.path.clear();
				continue;
			}

			auto s=x::fileattr::create(pi.path)->stat();

			pi.devino=std::make_pair(s.st_dev, s.st_ino);
			continue;
		}

		if (cmd == "ROOT" && !words.empty())
		{
			localmap[pi.devino].first[""]=words.front();
			continue;
		}

		if (cmd == "ROROOT" && !words.empty())
		{
			localmap[pi.devino].second[""]=words.front();
			continue;
		}

		std::string pfix=words.front();

		words.pop_front();

		if (cmd == "RW" || cmd == "RO")
		{
			if (pfix.size() == 0 ||
			    pfix.find('/') != std::string::npos)
			{
				throw EXCEPTION("Invalid namespace: "
						+ pfix);
			}

			if (!words.empty())
			{
				auto &maps=localmap[pi.devino];

				auto &s=cmd == "RW" ? maps.first:maps.second;

				s[pfix]=words.front();
				continue;
			}
		}
		throw EXCEPTION("Invalid command: " + cmd);
	} while (!i->eof());
}

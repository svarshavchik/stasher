/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "threadreport.H"
#include <algorithm>
#include <iomanip>
#include <sstream>

STASHER_NAMESPACE_START

class threadreportObj::sort_threads {

public:
	bool operator()(const x::ptr<singlethreadreportObj> &a,
			const x::ptr<singlethreadreportObj> &b)
	{
		return a->name < b->name;
	}
};

void threadreportObj::format(std::ostream &o)
{
	std::sort(reports.begin(), reports.end(), sort_threads());

	for (auto rep:reports)
	{
		o << std::endl << rep->name
		  << " (thread "
		  << std::showbase << std::hex
		  << std::setfill('0')
		  << rep->thread_id
		  << std::dec << std::noshowbase
		  << "):" << std::endl;

		std::istringstream i(rep->report);

		std::string line;

		while (line.clear(), !std::getline(i, line).fail())
		{
			o << "    " << line << std::endl;
		}
	}
}
STASHER_NAMESPACE_END

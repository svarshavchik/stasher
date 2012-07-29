/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "dirs.H"

STASHER_NAMESPACE_START

std::string privsockdir(const std::string &directory)
	noexcept
{
	return directory + "/privsocket";
}

std::string pubsockdir(const std::string &directory) noexcept
{
	return directory + "/pubsocket";
}

std::string privsockname(const std::string &directory) noexcept
{
	return privsockdir(directory) + "/socket";
}

std::string pubsockname(const std::string &directory) noexcept
{
	return pubsockdir(directory) + "/socket";
}

STASHER_NAMESPACE_END

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef dirs_H
#define dirs_H

#include <string>

#include "stasher/namespace.H"

STASHER_NAMESPACE_START

//! Return the name of the private socket directory in the given node directory
std::string privsockdir(const std::string &directory) noexcept;

//! Return the name of the public socket directory in the given node directory
std::string pubsockdir(const std::string &directory) noexcept;

//! Return the name of the private socket in the given node directory
std::string privsockname(const std::string &directory) noexcept;

//! Return the name of the public socket in the given node directory
std::string pubsockname(const std::string &directory) noexcept;

STASHER_NAMESPACE_END

#endif

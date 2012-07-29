/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "objuuidlist.H"
#include "objwriter.H"

x::property::value<size_t>
objuuidlistObj::default_chunksize(L"copychunksize", 16);

objuuidlistObj::objuuidlistObj() noexcept
{
}

objuuidlistObj::~objuuidlistObj() noexcept
{
}

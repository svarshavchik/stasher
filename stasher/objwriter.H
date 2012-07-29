/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef objwriter_H
#define objwriter_H

#include <x/obj.H>
#include <x/serialize.H>
#include <x/property_value.H>

#include <vector>

#include "stasher/namespace.H"

STASHER_NAMESPACE_START

//! Serialize objects to some output stream

//! \par The serialization function [SERIALIZE]
//!
//! The class defines a template serialize() function, that takes a reference
//! to an object, and serializes it.
//!
//! \par Buffering [BUFFER]
//!
//! The class instance maintains an internal buffer for the serialized stream.
//! The buffer's size is set by the constructor. When the internal buffer is
//! full, flush() gets called.
//!
//! flush() must be implemented in a subclass, and returns the number of bytes
//! written to the output stream, which must be at least 1.
//!
//! \par Manual flushing [FLUSH]
//!
//! buffered() returns an indication whether there's any serialized data in
//! the internal buffer that has not been flushed yet.
//!
//! pubflush() invokes flush() to flush anything that's left in the internal
//! buffer, and remove whatever flush() ends up flushing from the internal
//! buffer.

class objwriterObj : virtual public x::obj {

	//! Default buffer size

public:
	static x::property::value<size_t> default_bufsize;

private:
	//! Internal buffer that accumulated the serialized byte stream
	std::vector<char> buffer;

	//! Current size of the serialized byte stream
	size_t buf_ptr;

public:
	//! Iterator operator

	objwriterObj &operator++() noexcept
	{
		return *this;
	}

	//! Iterator operator

	objwriterObj &operator++(int) noexcept
	{
		return *this;
	}

	//! Iterator operator

	objwriterObj &operator*() noexcept
	{
		return *this;
	}

	//! Iterator operator

	objwriterObj &operator=(uint8_t c)
	{
		if (buf_ptr >= buffer.size())
			pubflush();

		buffer[buf_ptr++]=c;
		return *this;
	}

private:
	//! Internal serializer

	x::serialize::iterator<objwriterObj> ser_iter;

public:

	//! Constructor

	objwriterObj(size_t bufsize=default_bufsize.getValue());


	//! Destructor
	~objwriterObj() noexcept;

	//! Serialization function

	template<typename obj_type> void serialize(obj_type &obj)

	{
		ser_iter(obj);
	}

	//! Return an indication whether there's buffered serialized data.
	bool buffered() noexcept
	{
		return buf_ptr > 0;
	}

	//! Flush some or all of what's buffered.
	void pubflush();

private:

	//! Subclass must implement the flushing operation, and must flush at least one character

	virtual size_t flush(const char *ptr, size_t cnt)=0;
};

STASHER_NAMESPACE_END

#endif

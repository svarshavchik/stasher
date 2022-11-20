/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef stasher_objname_H
#define stasher_objname_H

#include <iterator>
#include <x/exception.H>

#include <stasher/namespace.H>

STASHER_NAMESPACE_START

//! Throw an exception: invalid object name

void invalid_object_name()
	__attribute__((noreturn));

//! Encode or validate an object name

//! The object name is defined by a beginning and an ending iterator.
//! The object name gets encoded into an output iterator, at the same
//! time checking its validity. An exception gets thrown if the object
//! name is not valid.

template<typename in_iter, typename out_iter>
out_iter encode_object_name(in_iter beg_iter, in_iter end_iter,
			    out_iter p_iter)
{
	while (1)
	{
		if (beg_iter == end_iter || *beg_iter == '/')
			invalid_object_name();

		while (1)
		{
			switch (*beg_iter) {
			case '.':
				*p_iter++='\\';
				*p_iter++='_';
				break;
			case '\\':
				*p_iter++='\\';
				*p_iter++='\\';
				break;
			default:
				*p_iter++=*beg_iter;
			}

			if (*beg_iter++ == '/')
				break;

			if (beg_iter == end_iter)
			{
				*p_iter++='.';
				*p_iter++='f';
				return p_iter;
			}
		}
	}
}

//! An iterator that counts the encoded length;

class object_name_length_iter {

	//! Counter
	size_t n;

public:
	typedef std::output_iterator_tag iterator_category;
	typedef void value_type;
	typedef void difference_type;
	typedef void pointer;
	typedef void iterator;

	//! Constructor
	object_name_length_iter() noexcept : n(0) {}

	//! Destructor
	~object_name_length_iter() {}

	//! Retrieve count
	operator size_t() const noexcept { return n; }

	//! Iterator operator
	object_name_length_iter &operator*() noexcept { return *this; }

	//! Iterator operator
	object_name_length_iter &operator++(int) noexcept
	{
		return *this;
	}

	//! Iterator operator
	object_name_length_iter &operator=(char c) noexcept
	{
		++n;
		return *this;
	}
};

//! Self explanatory

template<typename in_iter>
size_t encoded_object_name_length(in_iter beg_iter, in_iter end_iter)
{
	return encode_object_name(beg_iter, end_iter,
				  object_name_length_iter());
}

STASHER_NAMESPACE_END

#endif

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#ifndef spacemonitor_H
#define spacemonitor_H

#include <x/df.H>
#include <x/property_value.H>
#include <x/fmtsize.H>

//! Monitor available disk space

class spacemonitorObj : virtual public x::obj {

	//! Free space monitor
	x::df freespaceref;

	//! The "reserved::refresh property"
	static x::property::value<long> reserved_refresh;

	//! Initialized by reset_refresh_counter

	x::refcnt refresh_counter;

	//! Set refresh_counter from reserved_refresh()

	void reset_refresh_counter();


public:
	//! Constructor
	spacemonitorObj(const x::df &freespacerefArg);

	//! Destructor
	~spacemonitorObj();

	//! Reference handle for reserved space

	typedef x::df::base::reservation reservation;

	//! Pointer handle for reserved space

	typedef x::df::base::reservationptr reservationptr;

	//! Reserve some amount of space

	reservation reservespace_alloc(//! Number of allocation units
				       long alloc,

				       //! Number of inodes
				       long inodes);

	//! Calculate number of allocation units required for a file of given size

	long calculate_alloc(uint64_t filesize)
	{
		uint64_t frag=freespaceref->allocSize();

		return (filesize + frag - 1)/frag;
	}

	//! Calculate number of allocation units required for files of given sizes

	//! Given an iterator over file sizes, computes the total number of
	//! allocation units required.

	template<typename iter_type>
	long calculate_alloc(//! Beginning iterator
			     iter_type beg_iter,

			     //! Ending iterator
			     iter_type end_iter)
	{
		uint64_t frag=freespaceref->allocSize();
		long n=0;

		while (beg_iter != end_iter)
			n += (*beg_iter++ + frag - 1)/frag;

		return n;
	}

	//! Calculate number of inodes to reserve for a given object name

	static long calculate_inode(const std::string &objectname);


	//! Calculate number of inodes to reserve for a list of objects

	//! Given an iterator over object names, computes the total number of
	//! inodes required.

	template<typename iter_type>
	static long calculate_inode(//! Beginning iterator
				    iter_type beg_iter,

				    //! Ending iterator
				    iter_type end_iter)
	{
		long n=0;

		while (beg_iter != end_iter)
			n += calculate_inode(*beg_iter++);

		return n;
	}

	//! Return true if all reservations have used up all of the space

	bool outofspace();

	//! Return amount of estimated unreserved space, in megabytes.
	uint64_t freespacemb();

	//! Return amount of estimated unreserved inodes.
	uint64_t freeinodes();
};

//! A reference to a space monitor object

typedef x::ref<spacemonitorObj> spacemonitor;

//! A nullable pointer reference to a space monitor object

typedef x::ptr<spacemonitorObj> spacemonitorptr;

#endif

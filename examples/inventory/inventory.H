#ifndef inventory_H
#define inventory_H

#include <x/obj.H>
#include <x/ref.H>
#include <x/ptr.H>
#include <x/uuid.H>
#include <x/fd.H>
#include <x/fditer.H>
#include <x/serialize.H>
#include <x/deserialize.H>
#include <x/exception.H>

#include <map>
#include <iostream>

// An inventory of stuff

class inventoryObj : virtual public x::obj {

public:

	// This is the inventory, key product name, int is the number in stock.

	std::map<std::string, int> stock;

	// uuid of this object in the repository

	x::uuid uuid;

	// Default constructor

	inventoryObj() {}

	// Destructor
	~inventoryObj() {}

	// Copy constructor
	inventoryObj(const inventoryObj &o) : stock(o.stock), uuid(o.uuid)
	{
	}

	// Serialization function

	template<typename iter_type>
	void serialize(iter_type &iter)
	{
		iter(stock);
	}

	// Construct from an object in the repository

	inventoryObj(const x::uuid &uuidArg, const x::fd &fd) : uuid(uuidArg)
	{
		x::fdinputiter b(fd), e;

		x::deserialize::iterator<x::fdinputiter> iter(b, e);

		serialize(iter);
	}

	// The above constructor threw an exception, so the manager calls this
	// one. We should report an error somehow, the inventory object must be
	// corrupted, but this is just an example.

	inventoryObj(const x::uuid &uuidArg) : uuid(uuidArg)
	{
	}
};

typedef x::ref<inventoryObj> inventory;

typedef x::ptr<inventoryObj> inventoryptr;
#endif


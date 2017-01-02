#ifndef warehouses_H
#define warehouses_H

#include <stasher/current.H>
#include <stasher/client.H>
#include <stasher/manager.H>
#include "inventory.H"

class warehousesObj;
typedef x::ref<warehousesObj> warehouses_t;

// A warehouse is a versioned current object.

typedef stasher::current<inventoryptr, stasher::versionedptr<inventoryptr>
			 > warehouse_t;

// All of our warehouses are here.

class warehousesObj : virtual public x::obj {
public:
	warehousesObj() {}
	~warehousesObj() {}

	// Warehouse, by name.

	std::map<std::string, warehouse_t> warehouses;

	// When parsing command line options, create a warehouse dynamically,
	// from the command line list.
	void createwarehouse(const std::string &s)
	{
		if (warehouses.find(s) == warehouses.end())
			warehouses.insert(std::make_pair(s, warehouse_t
							 ::create()));
	}

	void inventory()
	{
		for (auto &warehouse:warehouses)
		{
			std::cout << warehouse.first << ":" << std::endl;

			warehouse_t::base::current_value_t::lock
				lock(warehouse.second->current_value);

			if (lock->value.null())
			{
				std::cout << "    (none)" << std::endl;
				continue;
			}

			std::cout << "    "
				  << std::setw(30) << std::left
				  << "Item"
				  << "   "
				  << std::setw(8) << std::right
				  << "Count" << std::setw(0)
				  << std::endl;

			std::cout << "    "
				  << std::setfill('-') << std::setw(30)
				  << ""
				  << "   "
				  << std::setw(8)
				  << "" << std::setw(0) << std::setfill(' ')
				  << std::endl;

			for (auto &item:lock->value->stock)
			{
				std::cout << "    "
					  << std::setw(30) << std::left
					  << item.first
					  << "   "
					  << std::setw(8) << std::right
					  << item.second << std::setw(0)
					  << std::endl;
			}
			std::cout << std::setw(75) << std::setfill('=') << ""
				  << std::setw(0) << std::setfill(' ')
				  << std::endl;
		}
	}
};

// Create the managed object subscriptions, and wait for the objects to get
// loaded from the database.

void load_warehouses(// Our warehouses
		     const warehouses_t &warehouses,

		     // The client
		     const stasher::client &client,

		     // The connection manager
		     const stasher::manager &manager,

		     // Mcguffins for managed object subscriptions are placed here
		     std::list<x::ref<x::obj> > &mcguffins);

#endif

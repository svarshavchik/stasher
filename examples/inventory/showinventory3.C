#include <stasher/client.H>
#include <stasher/manager.H>
#include <stasher/current.H>

#include <x/strtok.H>

#include <sstream>
#include <vector>

#include "inventory.H"

void show_inventory(const inventoryptr &, bool initial);
void status(stasher::req_stat_t status);

bool has_inventory(const inventoryptr &ptr,
		   const std::string &what,
		   int howmuch);

void showinventory(int argc, char **argv)
{
	if (argc < 2)
		return;

	auto client=stasher::client::base::connect();
	auto manager=stasher::manager::create();

	typedef stasher::current<inventoryptr> warehouse_t;
	warehouse_t warehouse=warehouse_t::create();

	x::ref<x::obj> mcguffin=warehouse->manage(manager, client, argv[1]);

	std::cout << "Enter for the current inventory, \"what\" \"howmuch\" to wait until we have it,"
		  << std::endl << "EOF to quit"
		  << std::endl;

	std::string dummy;

	while (!std::getline(std::cin, dummy).eof())
	{
		std::vector<std::string> words;

		x::strtok_str(dummy, " \t\r\n", words);

		warehouse_t::base::current_value_t::lock
			lock(warehouse->current_value);

		if (words.size() == 2)
		{
			int i=-1;

			std::istringstream(words[1]) >> i;

			if (i < 0)
			{
				std::cerr << "Eh?" << std::endl;
				continue;
			}

			std::cout << "Waiting for " << i << " " << words[0]
				  << std::endl;

			lock.wait([&lock, &words, i]
				  {
					  return has_inventory(lock->value,
							       words[0],
							       i);
				  });
		}
		show_inventory(lock->value, lock->isinitial);
		status(lock->connection_status);
	}
}

bool has_inventory(const inventoryptr &ptr,
		   const std::string &what,
		   int howmuch)
{
	if (ptr.null())
	{
		std::cout << "No inventory yet..." << std::endl;
		return false;
	}

	auto iter=ptr->stock.find(what);

	int wehave=iter == ptr->stock.end() ? 0:iter->second;

	std::cout << "We have " << wehave << " of them, now." << std::endl;
	return wehave >= howmuch;
}

void show_inventory(const inventoryptr &ptr, bool initial)
{
	std::cout << (initial ? "Current inventory:":"Updated inventory:")
		  << std::endl;

	if (ptr.null())
	{
		std::cout << "    (none)" << std::endl;
	}
	else
	{
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

		for (auto &item:ptr->stock)
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
			  << std::setw(0) << std::setfill(' ') << std::endl;
	}

}

void status(stasher::req_stat_t status)
{
	std::cout << "Connection status: "
		  << x::tostring(status) << std::endl;
}

int main(int argc, char **argv)
{
	try {
		showinventory(argc, argv);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		return 1;
	}
	return 0;
}

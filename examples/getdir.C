#include <iostream>
#include <stasher/client.H>

void showhier(int argc, char **argv)
{
	stasher::client client=stasher::client::base::connect();

	stasher::getdirresults results=client->getdir(argc < 2 ? "":argv[1]);

	if (results->status != stasher::req_processed_stat)
		throw EXCEPTION(x::tostring(results->status));

	for (auto &direntry:results->objects)
	{
		std::cout << direntry << std::endl;
	}
}

int main(int argc, char **argv)
{
	try {
		showhier(argc, argv);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}

	return 0;
}

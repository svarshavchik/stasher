/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "node.H"

#include <x/fd.H>
#include <x/options.H>
#include "stasherd.h"

class bareopts {

public:
	std::string directory;

	bool daemon;

	bareopts(int argc, char **argv)
	{
		objrepodoptions options;

		std::list<std::string> args(options.parse(argc, argv)->args);

		if (args.empty())
			throw EXCEPTION("Missing directory specification");

		directory=args.front();

		daemon=options.daemon->isSet();
	}
};

int main(int argc, char **argv)
{
	int rc=0;

	try {
		bareopts opts(argc, argv);
		std::pair<x::fdptr, x::fdptr> sock;

		if (opts.daemon)
		{
			sock=x::fd::base::pipe();

			pid_t p=fork();

			if (p < 0)
				throw SYSEXCEPTION("fork");

			if (p)
			{
				char rc=1;

				sock.second=x::fdptr();
				sock.first->read(&rc, 1);
				exit(rc);
			}

			sock.first=x::fdptr();
		}

		{
			if (chdir(opts.directory.c_str()) < 0)
				throw SYSEXCEPTION(opts.directory);

			x::filestat st(x::fileattr::create(".")->stat());

			if (setgid(st->st_gid) < 0 ||
			    setuid(st->st_uid) < 0)
				throw SYSEXCEPTION(opts.directory);

			node n(".");

			n.start(false);

			if (opts.daemon)
			{
				sock.second->write("", 1);
				sock.second=x::fdptr();

				int fd=open("/dev/null", O_RDWR);

				if (fd >= 0)
				{
					dup2(fd, 0);
					dup2(fd, 1);
					dup2(fd, 2);
					close(fd);
				}
			}
			n.wait();
		}
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		rc=1;
	}

	exit(rc);
	return (0);
}

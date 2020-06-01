/*
** Copyright 2012-2014 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "stasher/nodeinfo.H"
#include "stasher/objname.H"
#include <x/options.H>
#include <x/locale.H>
#include <x/wordexp.H>
#include <x/fditer.H>
#include <x/property_value.H>
#include <x/property_properties.H>
#include <x/property_save.H>
#include <x/deserialize.H>
#include <x/serialize.H>
#include <x/fmtsize.H>
#include <x/dir.H>
#include <x/sysexception.H>
#include <courier-unicode.h>

#include <iterator>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "stasher.h"
#include "stasherdpath.h"

// Out of tree, use <stasher/filename.H>

#include "stasher/client.H"
#include "stasher/contents.H"
#include "stasher/userhelo.H"
#include "nslist.H"

#include <set>
#include <map>

#if HAVE_READLINE_READLINE_H
#include <readline/readline.h>
#include <readline/history.h>
#endif

class cli {

	class subscriberHandlerObj : public STASHER_NAMESPACE::client::base::subscriberObj {

		std::multimap<std::string, std::string> updates;

	public:
		subscriberHandlerObj() {}
		~subscriberHandlerObj() {}

		void updated(const std::string &objname,
			     const std::string &suffix) override
		{
			std::lock_guard<std::mutex> lock(objmutex);

			updates.insert(std::make_pair(objname,
						      suffix));
		}

		void list()
		{
			std::lock_guard<std::mutex> lock(objmutex);

			for (auto update:updates)
			{
				std::cout << update.first
					  << update.second
					  << " updated"
					  << std::endl;
			}
			updates.clear();
		}
	};

	STASHER_NAMESPACE::clientptr client;
	std::string nodename;
	std::string clustername;
	bool adminconn;
	x::ptr<subscriberHandlerObj> subscriber;
	std::map<std::string, x::ptr<x::obj> > subscription_list;

	class currentns : virtual public x::obj {

	public:
		x::uuid nsmapuuid;
		nslist map;

		currentns(const x::uuid &nsmapuuidArg)
			: nsmapuuid(nsmapuuidArg)
		{
		}
		~currentns() {}

		class adddelinfo {
		public:

			std::string action;

			nsmap newnsmap;

			std::map<std::string, std::string> adds;
			std::set<std::string> unmaps;

			std::set<std::string> dels;

			std::string setlabel;
			bool unsetlabel;

			adddelinfo() : unsetlabel(false) {}
		};

		x::fd doadddel(const std::string &rwro,
			       const adddelinfo &info);

		x::fd domove(const std::string &rw,
			     const std::string &beforeafter,
			     const nsmap &move,
			     const nsmap &anchor);

	private:
		x::fd tofd();
	};

	x::ptr<currentns> ns;

	void do_disconnect()
	{
		client=STASHER_NAMESPACE::clientptr();
		ns=x::ptr<currentns>();

		if (interactive)
			std::cerr << "Disconnected from "
				  << nodename << std::endl;
		nodename="";
		clustername="";
		subscription_list.clear();
	}

	// Cluster metadata being edited.

	class editclusterObj : virtual public x::obj {

	public:
		STASHER_NAMESPACE::nodeinfomap info;

		editclusterObj() {}
		~editclusterObj() {}

		virtual void save(const STASHER_NAMESPACE::client::base::transaction &tran)
=0;

		class newcluster;
		class updcluster;

	protected:

		std::string serialize()
		{
			std::string s;

			typedef std::back_insert_iterator<std::string>
				ins_iter_t;

			ins_iter_t ins_iter(s);

			x::serialize::iterator<ins_iter_t> ser_iter(ins_iter);

			ser_iter(info);

			return s;
		}
	};

	x::ptr<editclusterObj> curcluster;

	static x::property::value<std::string> stasherd;

	void connected()
	{
		if (client.null())
			throw EXCEPTION("Not connected to a server");
	}

	void notconnected()
	{
		if (!client.null())
			throw EXCEPTION("Already connected to a server");
	}

	void newconnection(const STASHER_NAMESPACE::client &cl)
	{
		STASHER_NAMESPACE::userhelo helo=cl->gethelo();

		client=cl;
		curcluster=x::ptr<editclusterObj>();
		subscriber=x::ptr<subscriberHandlerObj>::create();

		if (interactive)
			std::cerr << "Connected to "
				  << helo.clustername
				  << ", node " << helo.nodename
				  << "." << std::endl << "Maximum "
				  << helo.limits.maxobjects
				  << " objects, "
				  << x::fmtsize(helo.limits.maxobjectsize)
				  << " aggregate object size, per transaction."
				  << std::endl
				  << "Maximum "
				  << helo.limits.maxsubs
				  << " concurrent subscriptions." << std::endl;
		nodename=helo.nodename;
		clustername=helo.clustername;
	}

	// By convention object names are coded in utf-8

	static std::string toutf8(const std::string &n)
	{
		return unicode::iconvert::convert(n, unicode_default_chset(),
						  unicode::utf_8);
	}

	// And now, the other way

	static std::string fromutf8(const std::string &n)
	{
		return unicode::iconvert::convert(n, unicode::utf_8,
						  unicode_default_chset());
	}

	static char needquote(char c)
	{
		return strchr("\"'|&;<>(){}#~[]*?$`\\", c) != 0 ||
			(unsigned char)c < ' ';
	}

	static std::string quote(const std::string &s)
	{
		if (s.size() &&
		    std::find_if(s.begin(), s.end(), std::ptr_fun(needquote))
		    == s.end())
			return s;

		std::ostringstream o;

		o << "\"";
		for (std::string::const_iterator b=s.begin(), e=s.end();
		     b != e; ++b)
		{
			if (needquote(*b))
				o << "\\";
			o << *b;
		}
		o << "\"";
		return o.str();
	}

public:
	x::locale locale;
	bool interactive;

	cli() : locale(x::locale::create("")), interactive(isatty(0)) {}


	// Node names have the cluster name automatically affixed. For clarity,
	// strip it off.

	std::string getNodename() noexcept
	{
		std::string s=nodename;

		std::string d="." + clustername;

		if (s.size() > d.size() &&
		    s.substr(s.size()-d.size()) == d)
			s=s.substr(0, s.size() - d.size());

		return s;
	}

	bool getline(std::string &line, bool continue_prompt)
	{
		if (!subscriber.null())
			subscriber->list();

		std::string prompt=continue_prompt ? ": "
			: (getNodename() + "> ");

		line.clear();

#if HAVE_READLINE_READLINE_H
		if (interactive)
		{
			char *p=readline(prompt.c_str());

			if (!p)
				return false;

			line=p;
			free(p);
		}
		else
		{
#else
			if (interactive)
				std::cout << prompt;
#endif

			std::getline(std::cin, line);

			if (line.empty() && std::cin.eof())
				return false;
#if HAVE_READLINE_READLINE_H
		}
#endif

		return true;
	}

	void command(const std::string &s);
	void command(std::list<std::string> &args);

	std::string getnodedir(const std::list<std::string> &args);
	void connect(const std::string &path);
	void admin(const std::string &path);
	void start(const std::string &path);
	void resign();
	void halt();
	void stop();
	void status();
	void disconnect();
	void put(const std::list<std::string> &args);
	void uuids(const std::list<std::string> &args);
	void rmrf(const std::list<std::string> &args);
	void dormrf(const std::vector<std::string> &objs);
	void get(const std::list<std::string> &args);
	void save(const std::list<std::string> &args);
	void dir(const std::list<std::string> &args);
	void getprops(const std::list<std::string> &args);
	void setprop(const std::string &propname, const std::string &propvalue);
	void resetprop(const std::string &propname);
	void setlimits(const std::string &maxobjects,
		       const std::string &maxobjectsize);
	void new_or_update_or_delete(const std::map<std::string, std::string> &newvalues,
				     const std::set<std::string> &remove);

	void editcluster(std::list<std::string> &args);
	void cluster(std::list<std::string> &args);
	void savecluster(std::list<std::string> &args);
	void subscribe(std::list<std::string> &args);
	void unsubscribe(std::list<std::string> &args);
	void namespacecmd(std::list<std::string> &args);

private:
	void namespaceupdate(const STASHER_NAMESPACE::client::base::transaction &tran);
	void namespaceshow();
	void namespaceparseid(std::list<std::string> &args,
			      nsmap &nsid);
	void namespaceshowmapentry(const nsmap &b);

	x::ptr<currentns> loadns();

	void savecluster(bool force);

	void do_command(std::list<std::string> &args);

	void put_syntax_error();
	void namespace_syntax_error();
	void validate_object_name(const std::string &n);
};

class cli::editclusterObj::newcluster : public cli::editclusterObj {

public:
	newcluster() {}
	~newcluster() {}

	void save(const STASHER_NAMESPACE::client::base::transaction &tran)
		override
	{
		tran->newobj(STASHER_NAMESPACE::client::base::clusterconfigobj, serialize());
	}
};

class cli::editclusterObj::updcluster : public cli::editclusterObj {

public:
	x::uuid olduuid;

	updcluster(const x::uuid &olduuidArg)
		: olduuid(olduuidArg) {}

	~updcluster() {}

	void save(const STASHER_NAMESPACE::client::base::transaction &tran)
		override
	{
		tran->updobj(STASHER_NAMESPACE::client::base::clusterconfigobj, olduuid,
			     serialize());
	}
};

x::property::value<std::string> cli::stasherd("objrepo::stasherd",
					      stasherdpath);

void cli::command(const std::string &s)
{
	std::list<std::string> l;

	{
		std::vector<std::string> v;

		x::wordexp(s, v, WRDE_SHOWERR);

		l.insert(l.end(), v.begin(), v.end());
	}

	try {
		command(l);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
	}
}

void cli::command(std::list<std::string> &args)
{
	try {
		do_command(args);
	} catch (...)
	{
		// Try to detect dead connections, and clear them out

		if (!client.null() && client->disconnected())
			do_disconnect();
		throw;
	}
}

void cli::do_command(std::list<std::string> &args)
{
	if (args.empty())
		return;

	std::string cmd=locale->tolower(args.front());
	args.pop_front();

	if (cmd == "put")
	{
		put(args);
		return;
	}

	if (cmd == "get")
	{
		get(args);
		return;
	}

	if (cmd == "uids")
	{
		uuids(args);
		return;
	}

	if (cmd == "rmrf")
	{
		rmrf(args);
		return;
	}

	if (cmd == "save")
	{
		save(args);
		return;
	}

	if (cmd == "dir")
	{
		dir(args);
		return;
	}

	if (cmd == "getprops")
	{
		getprops(args);
		return;
	}

	if (cmd == "editcluster")
	{
		editcluster(args);
		return;
	}

	if (cmd == "cluster")
	{
		cluster(args);
		return;
	}

	if (cmd == "savecluster")
	{
		savecluster(args);
		return;
	}

	if (cmd == "sub")
	{
		subscribe(args);
		return;
	}

	if (cmd == "unsub")
	{
		unsubscribe(args);
		return;
	}

	if (cmd == "namespace")
	{
		namespacecmd(args);
		return;
	}

	if (cmd == "connect")
	{
		connect(getnodedir(args));
		return;
	}
	if (cmd == "admin")
	{
		admin(getnodedir(args));
		return;
	}

	if (!args.empty())
	{
		std::string arg=args.front();

		args.pop_front();

		if (cmd == "start")
		{
			start(arg);
			return;
		}

		if (cmd == "resetprop")
		{
			resetprop(arg);
			return;
		}

		if (!args.empty())
		{
			std::string arg2=args.front();

			args.pop_front();

			if (cmd == "setprop")
			{
				setprop(arg, arg2);
				return;
			}

			if (cmd == "setlimits")
			{
				setlimits(arg, arg2);
				return;
			}
		}
	}

	if (cmd == "resign")
	{
		resign();
		return;
	}

	if (cmd == "stop")
	{
		stop();
		return;
	}

	if (cmd == "disconnect")
	{
		disconnect();
		return;
	}

	if (cmd == "status")
	{
		status();
		return;
	}

	if (cmd == "halt")
	{
		halt();
		return;
	}
	throw EXCEPTION("Unknown command: " + cmd);
}

std::string cli::getnodedir(const std::list<std::string> &args)
{
	if (!args.empty())
		return args.front();

	std::set<std::string> dirs;

	STASHER_NAMESPACE::client::base::defaultnodes(dirs);

	if (dirs.empty())
		throw EXCEPTION("There are no nodes installed in the default node directory");

	auto b=dirs.begin();

	std::string s=*b;

	dirs.erase(b);

	if (!dirs.empty())
		throw EXCEPTION("More than one node installed in the default node directory");

	return s;
}

void cli::connect(const std::string &path)
{
	notconnected();

	newconnection(STASHER_NAMESPACE::client::base::connect(path));
	adminconn=false;
}

void cli::admin(const std::string &path)
{
	notconnected();

	newconnection(STASHER_NAMESPACE::client::base::admin(path));
	adminconn=true;
}

void cli::start(const std::string &path)
{
	pid_t p;

	notconnected();

	p=fork();

	if (p < 0)
		throw SYSEXCEPTION("fork");

	if (p == 0)
	{
		std::string properties=path + "/properties";

		if (access(properties.c_str(), R_OK) == 0)
			setenv("PROPERTIES", properties.c_str(), 1);

		std::string prog=stasherd.get();

		execl(prog.c_str(), prog.c_str(), "--daemon", path.c_str(),
		      NULL);

		perror(prog.c_str());
		exit(1);
	}

	// Wait for a 0 exit status, indicating that the server has started

	int status;

	while (wait(&status) != p)
		;

	if (status)
		throw EXCEPTION("Server start failed");
	admin(path);
}

void cli::resign()
{
	connected();

	STASHER_NAMESPACE::resignresults results=client->resign();

	if (!results->status)
		throw EXCEPTION("Resignation request failed");

	if (interactive)
		std::cout
			<< "Server resigned, new master is " << results->master
			<< std::endl;
}

void cli::stop()
{
	connected();
	client->shutdown();
	disconnect();
}

void cli::status()
{
	connected();

	std::cout << client->getserverstatus()->report << std::flush;
}

void cli::disconnect()
{
	connected();
	do_disconnect();
}

void cli::halt()
{
	connected();

	auto res=client->haltrequest();

	std::cout << res->message << std::endl;

	if (res->halted)
		do_disconnect();
}

void cli::setlimits(const std::string &maxobjects,
		    const std::string &maxobjectsize)
{
	static const char usage[]="Usage: SETLIMITS maxobjects maxsize";

	std::map<std::string, std::string> replace;
	std::set<std::string> remove;

	if (locale->tolower(maxobjects) == "default")
	{
		remove.insert(STASHER_NAMESPACE::client::base
			      ::maxobjects);
	}
	else
	{
		size_t maxobjects_n;

		std::istringstream i(maxobjects);

		i >> maxobjects_n;

		if (i.fail())
			throw EXCEPTION(usage);

		if (maxobjects_n < 5 || maxobjects_n > 100)
		{
			throw EXCEPTION("Maximum number of objects must be between 5 and 100");
		}

		replace[STASHER_NAMESPACE::client::base::maxobjects]=
			({
				std::ostringstream o;

				o << maxobjects_n;

				o.str();
			});
	}

	if (locale->tolower(maxobjectsize) == "default")
	{
		remove.insert(STASHER_NAMESPACE::client::base
			      ::maxobjectsize);
	}
	else
	{
		auto maxsize=x::parsesize(maxobjectsize,
					  x::locale::base::environment());


		if (maxsize < 1024)
		{
			throw EXCEPTION("Maximum object size must be at least 1Kb, of course");
		}

		if (maxsize > 1024L * 1024 * 1024)
			throw EXCEPTION("Maximum object size cannot exceed one gigabyte");


		replace[STASHER_NAMESPACE::client::base::maxobjectsize]=
			({
				std::ostringstream o;

				o << maxsize;

				o.str();
			});
	}

	new_or_update_or_delete(replace, remove);
}

void cli::new_or_update_or_delete(const std::map<std::string, std::string> &newvalues,
				  const std::set<std::string> &removevalues)
{
	connected();

	STASHER_NAMESPACE::client::base::getreq req
		=STASHER_NAMESPACE::client::base::getreq::create();

	req->objects=removevalues;
	for (auto values:newvalues)
		req->objects.insert(values.first);

	STASHER_NAMESPACE::contents contents=client->get(req)->objects;

	if (!contents->succeeded)
		throw EXCEPTION(contents->errmsg);

	STASHER_NAMESPACE::client::base::transaction
		tran=STASHER_NAMESPACE::client::base::transaction::create();

	for (auto &values:newvalues)
	{
		auto iter=contents->find(values.first);

		if (iter == contents->end())
		{
			tran->newobj(values.first, values.second);
		}
		else
		{
			tran->updobj(values.first, iter->second.uuid,
				     values.second);
		}
	}

	for (auto &value:removevalues)
	{
		auto iter=contents->find(value);

		if (iter != contents->end())
			tran->delobj(value, iter->second.uuid);
	}

	STASHER_NAMESPACE::putresults results=client->put(tran);

	if (results->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION(x::to_string(results->status));
}

void cli::put(const std::list<std::string> &args)
{
	connected();

	STASHER_NAMESPACE::client::base::transaction
		tran=STASHER_NAMESPACE::client::base::transaction::create();

	for (std::list<std::string>::const_iterator
		     b=args.begin(), e=args.end(); b != e; )
	{
		std::string cmd=locale->tolower(*b);

		std::string content_str;
		x::fdptr content_fd;

		if (++b == e)
			put_syntax_error();

		std::string name=toutf8(*b++);

		x::uuid uuid;

		if (cmd != "new")
		{
			bool good_uuid=false;

			try {
				size_t p=name.rfind(':');

				if (p != name.npos)
				{
					uuid=name.substr(p+1);
					name=name.substr(0, p);
					good_uuid=true;
				}
			} catch (...) {
			}

			if (!good_uuid)
				put_syntax_error();
		}

		if (cmd == "del")
		{
			tran->delobj(name, uuid);
			continue;
		}

		if (b == e)
			put_syntax_error();

		std::string src=locale->tolower(*b);

		if (++b == e)
			put_syntax_error();

		if (src == "value")
		{
			content_str=*b;
		}
		else
			content_fd=x::fd::base::open(*b, O_RDONLY);
		++b;

		if (cmd == "upd")
		{
			if (!content_fd.null())
				tran->updobj(name, uuid, content_fd);
			else
				tran->updobj(name, uuid, content_str);
		}
		else if (cmd == "new")
		{
			if (!content_fd.null())
				tran->newobj(name, content_fd);
			else
				tran->newobj(name, content_str);
		}
		else put_syntax_error();
	}

	STASHER_NAMESPACE::putresults results=client->put(tran);

	if (results->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION(x::to_string(results->status));

	if (interactive)
		std::cout << "New object uuid: ";
	std::cout << x::to_string(results->newuuid) << std::endl;
}

void cli::put_syntax_error()
{
	throw EXCEPTION("Syntax error, usage:\n\n"
			"PUT ( (DEL name:uuid | NEW name | UPD name:uuid)\n"
			"      (VALUE literal | FILE filename) )+");
}

void cli::uuids(const std::list<std::string> &args)
{
	connected();

	STASHER_NAMESPACE::client::base::getreq req=STASHER_NAMESPACE::client::base::getreq::create();

	std::list<std::string> args_utf8;

	for (std::list<std::string>::const_iterator
		     b=args.begin(), e=args.end(); b != e; ++b)
	{
		args_utf8.push_back(toutf8(*b));
	}

	req->objects.insert(args_utf8.begin(), args_utf8.end());

	STASHER_NAMESPACE::contents contents=client->get(req)->objects;

	if (!contents->succeeded)
		throw EXCEPTION(contents->errmsg);

	STASHER_NAMESPACE::contents::base::map_t &values= *contents;

	for (std::list<std::string>::const_iterator b=args_utf8.begin(),
		     e=args_utf8.end(); b != e; ++b)
	{
		STASHER_NAMESPACE::contents::base::map_t::const_iterator p=values.find(*b);

		if (p == values.end())
		{
			std::cout << fromutf8(*b) << ":" << std::endl;
			continue;
		}

		std::cout << fromutf8(*b)
			  << ":" << x::to_string(p->second.uuid)
			  << std::endl;
	}
}

void cli::get(const std::list<std::string> &args)
{
	connected();

	STASHER_NAMESPACE::client::base::getreq req=STASHER_NAMESPACE::client::base::getreq::create();

	std::list<std::string> args_utf8;

	for (std::list<std::string>::const_iterator
		     b=args.begin(), e=args.end(); b != e; ++b)
	{
		args_utf8.push_back(toutf8(*b));
	}

	req->openobjects=true;
	req->objects.insert(args_utf8.begin(), args_utf8.end());

	STASHER_NAMESPACE::contents contents=client->get(req)->objects;

	if (!contents->succeeded)
		throw EXCEPTION(contents->errmsg);

	STASHER_NAMESPACE::contents::base::map_t &values= *contents;

	for (std::list<std::string>::const_iterator b=args_utf8.begin(),
		     e=args_utf8.end(); b != e; ++b)
	{
		STASHER_NAMESPACE::contents::base::map_t::const_iterator p=values.find(*b);

		if (p == values.end())
		{
			std::cout << fromutf8(*b) << ":" << std::endl;
			continue;
		}

		std::cout << fromutf8(*b)
			  << ":" << x::to_string(p->second.uuid)
			  << std::endl;
	}

	std::ostreambuf_iterator<char> o_iter(std::cout);

	for (std::list<std::string>::const_iterator b=args_utf8.begin(),
		     e=args_utf8.end(); b != e; ++b)
	{
		STASHER_NAMESPACE::contents::base::map_t::const_iterator p=values.find(*b);

		if (p == values.end())
			continue;

		x::istream i=p->second.fd->getistream();

		std::copy(std::istreambuf_iterator<char>(*i),
			  std::istreambuf_iterator<char>(),
			  std::ostreambuf_iterator<char>(std::cout));

		std::cout << std::endl;
	}
}

void cli::save(const std::list<std::string> &args)
{
	connected();

	STASHER_NAMESPACE::client::base::getreq req=STASHER_NAMESPACE::client::base::getreq::create();

	req->openobjects=true;

	std::map<std::string, std::string> filenamemap;

	for (std::list<std::string>::const_iterator b=args.begin(),
		     e=args.end(); b != e; ++b)
	{
		std::string n=toutf8(*b);

		req->objects.insert(n);

		if (++b == e)
			throw EXCEPTION("odd number of words to get");
		filenamemap[n]=*b;
	}

	STASHER_NAMESPACE::contents contents=client->get(req)->objects;

	if (!contents->succeeded)
		throw EXCEPTION(contents->errmsg);

	STASHER_NAMESPACE::contents::base::map_t &values= *contents;

	for (std::map<std::string, std::string>::const_iterator
		     b=filenamemap.begin(), e=filenamemap.end(); b != e; ++b)
	{
		STASHER_NAMESPACE::contents::base::map_t::const_iterator
			p=values.find(b->first);

		if (p == values.end())
		{
			std::cout << fromutf8(b->first)
				  << ":" << std::endl;
			continue;
		}

		std::cout << fromutf8(b->first)
			  << ":" << x::to_string(p->second.uuid)
			  << std::endl;

		x::fd ofile=x::fd::create(b->second);

		x::istream i=p->second.fd->getistream();
		x::ostream o=ofile->getostream();

		std::copy(std::istreambuf_iterator<char>(*i),
			  std::istreambuf_iterator<char>(),
			  std::ostreambuf_iterator<char>(*o));

		(*o) << std::flush;
		if (o->bad() || o->fail())
			throw EXCEPTION(fromutf8(b->first));
		ofile->close();
	}
}

void cli::dir(const std::list<std::string> &args)
{
	connected();

	STASHER_NAMESPACE::getdirresults res=client->getdir(toutf8(args.empty() ? ""
							 : args.front()));

	for (std::set<std::string>::const_iterator b=res->objects.begin(),
		     e=res->objects.end(); b != e; ++b)
		if (b->substr(b->size()-1) == "/")
			std::cout << fromutf8(*b) << std::endl;

	for (std::set<std::string>::const_iterator b=res->objects.begin(),
		     e=res->objects.end(); b != e; ++b)
		if (b->substr(b->size()-1) != "/")
			std::cout << fromutf8(*b) << std::endl;

	if (res->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION(x::to_string(res->status));
}

void cli::rmrf(const std::list<std::string> &args)
{
	connected();

	std::list<std::string> dirs;

	if (args.empty())
	{
		if (adminconn)
			throw EXCEPTION("Are you crazy?");

		dirs.push_front("");
	}
	else
		for (auto dir : args)
			dirs.push_back(toutf8(dir));

	std::vector<std::string> objlist;

	size_t max=client->gethelo().limits.maxobjects;

	while (!dirs.empty())
	{
		STASHER_NAMESPACE::getdirresults res=
			client->getdir(dirs.front());

		dirs.pop_front();

		for (auto object : res->objects)
		{
			if (object.substr(object.size()-1) == "/")
			{
				dirs.push_back(object.substr(0,
							     object.size()-1));
				continue;
			}

			if (objlist.size() >= max)
			{
				dormrf(objlist);
				objlist.clear();
			}

			objlist.push_back(object);
		}
	}

	if (objlist.size() > 0)
		dormrf(objlist);
}

void cli::dormrf(const std::vector<std::string> &objs)
{
 again:

	STASHER_NAMESPACE::client::base::getreq req
		=STASHER_NAMESPACE::client::base::getreq::create();

	req->objects.insert(objs.begin(), objs.end());

	STASHER_NAMESPACE::contents contents=client->get(req)->objects;

	if (!contents->succeeded)
		throw EXCEPTION(contents->errmsg);

	if (contents->empty())
		return;

	STASHER_NAMESPACE::client::base::transaction deltran=
		STASHER_NAMESPACE::client::base::transaction::create();

	for (auto objlist : *contents)
		deltran->delobj(objlist.first, objlist.second.uuid);

	STASHER_NAMESPACE::putresults results=client->put(deltran);

	if (results->status == STASHER_NAMESPACE::req_rejected_stat)
		goto again;

	if (results->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION(x::to_string(results->status));

	if (interactive)
		for (auto name : objs)
			std::cout << "Removed " << fromutf8(name)
				  << std::endl;
}

void cli::getprops(const std::list<std::string> &args)
{
	connected();

	STASHER_NAMESPACE::getpropsresults res=client->getprops();

	bool all=false;

	std::set<std::string> wanted;

	for (std::list<std::string>::const_iterator b=args.begin(),
		     e=args.end(); b != e; ++b)
	{
		std::string src=locale->tolower(*b);

		if (src == "/all")
		{
			all=true;
			continue;
		}

		// Standardize the property name

		std::list<std::string> hier;
		x::property::parsepropname(b->begin(), b->end(), hier,
					   locale);
		wanted.insert(x::property::combinepropname(hier));
	}

	for (std::map<std::string, std::string>
		     ::iterator b=res->properties.begin(),
		     e=res->properties.end(), p; (p=b) != e; b=p)
	{
		++p;

		if (x::property::islibraryprop(b->first))
		{
			if (!all)
			{
				res->properties.erase(b);
				continue;
			}
		}

		if (!wanted.empty() && wanted.find(b->first) == wanted.end())
		{
			res->properties.erase(b);
			continue;
		}
	}

	x::property::save_properties(res->properties,
				     std::ostreambuf_iterator<char>(std::cout),
				     locale);
}

void cli::setprop(const std::string &propname, const std::string &propvalue)
{
	connected();

	STASHER_NAMESPACE::setpropresults res=
		client->setprop(propname, propvalue);

	if (!res->succeeded)
		throw EXCEPTION(res->errmsg);

	std::cout << "Property set, new value: "
		  << x::to_string(res->newvalue, locale) << std::endl;
}

void cli::resetprop(const std::string &propname)
{
	connected();

	STASHER_NAMESPACE::resetpropresults res=
		client->resetprop(propname);

	std::cout << res->resultmsg << std::endl;
}

// ===========================================================================

void cli::editcluster(std::list<std::string> &args)
{
	connected();

	bool forceflag=false;
	bool newflag=false;

	while (!args.empty())
	{
		std::string cmd=locale->tolower(args.front());

		if (cmd == "force")
		{
			forceflag=true;
		}
		else if (cmd == "new")
		{
			newflag=true;
		}
		else break;

		args.pop_front();
	}

	STASHER_NAMESPACE::client::base::getreq req=STASHER_NAMESPACE::client::base::getreq::create();

	req->openobjects=true;
	req->admin=forceflag;
	req->objects.insert(STASHER_NAMESPACE::client::base::clusterconfigobj);

	STASHER_NAMESPACE::contents contents=client->get(req)->objects;

	if (!contents->succeeded)
		throw EXCEPTION(contents->errmsg);

	STASHER_NAMESPACE::contents::base::map_t &values= *contents;

	STASHER_NAMESPACE::contents::base::map_t::iterator
		p=values.find(STASHER_NAMESPACE::client::base::clusterconfigobj);

	if (p == values.end())
	{
		curcluster=x::ptr<editclusterObj::newcluster>::create();
	}
	else
	{
		x::uuid uuid=p->second.uuid;

		curcluster=x::ptr<editclusterObj::updcluster>
			::create(p->second.uuid);

		if (!newflag)
		{
			x::fd::base::inputiter beg_iter(p->second.fd), end_iter;

			x::deserialize::iterator<x::fd::base::inputiter>
				deserialize_iter(beg_iter, end_iter);

			deserialize_iter(curcluster->info);
		}
	}

	bool saveit=!args.empty();

	cluster(args);

	if (saveit)
		savecluster(forceflag);
}

void cli::cluster(std::list<std::string> &args)
{
	connected();

	std::string nodename;
	std::string op;

	if (curcluster.null())
		throw EXCEPTION("Cluster metadata not loaded, use the EDITCLUSTER command first");

	while (!args.empty())
	{
		std::string cmd=locale->tolower(args.front());

		args.pop_front();

		if (cmd == "addnode" && !args.empty())
		{
			std::vector<STASHER_NAMESPACE::nodeinfomap::iterator> ordervec;

			STASHER_NAMESPACE::nodeinfo::loadnodeorder(curcluster->info,
							 ordervec);
			std::pair<STASHER_NAMESPACE::nodeinfomap::iterator, bool>
				res=curcluster->info
				.insert(std::make_pair((nodename=args.front()
							+ "."
							+ clustername),
						       STASHER_NAMESPACE::nodeinfo()));

			if (!res.second)
				throw EXCEPTION(args.front()
						+ " already exists");

			args.pop_front();
			op="";
			ordervec.push_back(res.first);
			STASHER_NAMESPACE::nodeinfo::savenodeorder(ordervec);
			continue;
		}
		if (cmd == "update" && !args.empty())
		{
			if (curcluster->info.find(nodename=args.front() + "."
						  + clustername)
			    == curcluster->info.end())
				throw EXCEPTION(args.front()
						+ " does not exist");

			args.pop_front();
			op="";
			continue;
		}
		if (cmd == "delnode" && !args.empty())
		{
			STASHER_NAMESPACE::nodeinfomap::iterator iter=
				curcluster->info.find(args.front() + "."
						      + clustername);

			if (iter == curcluster->info.end())
				throw EXCEPTION(args.front()
						+ " does not exist");

			curcluster->info.erase(iter);

			std::vector<STASHER_NAMESPACE::nodeinfomap::iterator> ordervec;

			STASHER_NAMESPACE::nodeinfo::loadnodeorder(curcluster->info,
							 ordervec);

			STASHER_NAMESPACE::nodeinfo::savenodeorder(ordervec);
			args.pop_front();
			nodename="";
			op="";
			continue;
		}

		if (cmd == "reorder" && !args.empty())
		{
			std::vector<STASHER_NAMESPACE::nodeinfomap::iterator> ordervec;

			STASHER_NAMESPACE::nodeinfo::loadnodeorder(curcluster->info,
							 ordervec);

			size_t node_p=ordervec.size(),
				anchor_p=ordervec.size();

			std::string node=args.front() + "." + clustername;

			args.pop_front();

			bool done=false;

			while (1)
			{
				if (args.empty())
					break;

				cmd=locale->tolower(args.front());

				if (cmd != "before" && cmd != "after")
					break;

				args.pop_front();

				if (args.empty())
					break;

				std::string anchor=args.front() + "."
					+ clustername;
				args.pop_front();

				for (size_t i=0; i<ordervec.size(); ++i)
				{
					if (ordervec[i]->first == node)
						node_p=i;
					if (ordervec[i]->first == anchor)
						anchor_p=i;
				}

				if (node_p == ordervec.size())
					throw EXCEPTION(node + ": not found");

				if (anchor_p == ordervec.size())
					throw EXCEPTION(anchor + ": not found");

				done=true;
				break;
			}

			if (!done)
				throw EXCEPTION("Specify {node} [before|after] {node} to reoder");

			if (node_p == anchor_p)
				throw EXCEPTION("What?");

			ordervec.insert(ordervec.begin()+anchor_p+
					(cmd == "before" ? 0:1),
					ordervec[node_p]);

			if (node_p > anchor_p)
				++node_p;

			ordervec.erase(ordervec.begin() + node_p);
			STASHER_NAMESPACE::nodeinfo::savenodeorder(ordervec);
			continue;
		}
		if (cmd == "set" || cmd == "unset" || cmd == "add")
		{
			op=cmd;
			continue;
		}

		if (nodename.size() > 0)
		{
			typedef std::pair
				<STASHER_NAMESPACE::nodeinfo::options_t::iterator,
				 STASHER_NAMESPACE::nodeinfo::options_t::iterator>
				equal_range_t;

			STASHER_NAMESPACE::nodeinfo &info=curcluster->info[nodename];

			if (op == "set" || op == "unset")
			{
				equal_range_t equal_range=
					info.options.equal_range(cmd);

				info.options.erase(equal_range.first,
						   equal_range.second);
			}

			if (op == "set")
			{
				std::string::size_type pos=cmd.find('=');

				if (pos  == std::string::npos)
				{
					info.options
						.insert(std::make_pair(cmd,
								       "1"));

				}
				else
					info.options.insert
						(std::make_pair
						 (cmd.substr(0, pos),
						  cmd.substr(pos+1)));
				continue;
			}

			if (op == "add")
			{
				std::string::size_type pos=cmd.find('=');

				if (pos == std::string::npos)
					throw EXCEPTION("name=value required");
				info.options.insert(std::make_pair
						    (cmd.substr(0, pos),
						     cmd.substr(pos+1)));
				continue;
			}

			if (op == "unset")
				continue;
		}

		throw EXCEPTION("Unknown command: " + cmd);
	}

	if (!interactive)
		return;

	std::vector<STASHER_NAMESPACE::nodeinfomap::iterator> ordervec;

	STASHER_NAMESPACE::nodeinfo::loadnodeorder(curcluster->info, ordervec);

	if (ordervec.empty())
	{
		std::cout << "Empty node list" << std::endl;
		return;
	}

	std::string d="." + clustername;

	for (std::vector<STASHER_NAMESPACE::nodeinfomap::iterator>::const_iterator
		     nb=ordervec.begin(), ne=ordervec.end(); nb != ne;
	     ++nb)
	{
		STASHER_NAMESPACE::nodeinfomap::iterator nodeEntry=*nb;

		std::string n=nodeEntry->first;

		if (n.size() > d.size() && n.substr(n.size() - d.size()) == d)
			n=n.substr(0, n.size() - d.size());

		std::cout << n << ":" << std::endl;

		const STASHER_NAMESPACE::nodeinfo &info= nodeEntry->second;

		for (STASHER_NAMESPACE::nodeinfo::options_t::const_iterator
			     b=info.options.begin(),
			     e=info.options.end(); b != e; ++b)
		{
			if (x::chrcasecmp::str_equal_to()
			    (b->first,
			     STASHER_NAMESPACE::nodeinfo::ordernum_option))
				continue;

			std::cout << "    "
				  << x::locale::base::utf8()
				->toupper(b->first);

			if (b->second != "1")
				std::cout << "=" << b->second;
			std::cout << std::endl;
		}
		std::cout << std::endl;
	}
}

void cli::savecluster(std::list<std::string> &args)
{
	connected();

	bool forceflag=false;

	while (!args.empty())
	{
		std::string cmd=locale->tolower(args.front());

		if (cmd == "force")
			forceflag=true;
		else throw EXCEPTION("Unknown: " + args.front());

		args.pop_front();
	}

	savecluster(forceflag);
}

void cli::savecluster(bool forceflag)
{
	if (curcluster.null())
		throw EXCEPTION("Cluster metadata not loaded");

	if (!curcluster->info.empty() &&
	    curcluster->info.find(nodename) == curcluster->info.end())
		throw EXCEPTION("Cluster metadata must include this node");

	for (STASHER_NAMESPACE::nodeinfomap::iterator b=curcluster->info.begin(),
		     e=curcluster->info.end(); b != e; ++b)
		if (b->second.options.find(STASHER_NAMESPACE::nodeinfo::host_option)
		    == b->second.options.end())
			throw EXCEPTION(b->first
					+ ": required setting missing: "
					+ STASHER_NAMESPACE::nodeinfo::host_option);

	STASHER_NAMESPACE::client::base::transaction
		tran=STASHER_NAMESPACE::client::base::transaction::create();

	tran->admin=forceflag;
	curcluster->save(tran);

	STASHER_NAMESPACE::putresults results=client->put(tran);

	if (results->status != STASHER_NAMESPACE::req_processed_stat)
		throw EXCEPTION(x::to_string(results->status));

	auto newmeta=x::ptr<editclusterObj::updcluster>
		::create(results->newuuid);

	newmeta->info=curcluster->info;
	curcluster=newmeta;
}

void cli::subscribe(std::list<std::string> &args)
{
	connected();

	for (std::list<std::string>::const_iterator b=args.begin(),
		     e=args.end(); b != e; ++b)
	{
		std::string n= *b;

		STASHER_NAMESPACE::subscriberesults res=client->subscribe(n, subscriber);

		if (res->status != STASHER_NAMESPACE::req_processed_stat)
			throw EXCEPTION(x::to_string(res->status));

		subscription_list[n]=res->mcguffin;
		std::cout << "Subscribed to " << n << std::endl;
	}
}

void cli::unsubscribe(std::list<std::string> &args)
{
	connected();

	for (std::list<std::string>::const_iterator b=args.begin(),
		     e=args.end(); b != e; ++b)
		subscription_list.erase(*b);
}

void cli::namespace_syntax_error()
{
	throw EXCEPTION("Syntax error, usage:\n"
			"\n"
			"NAMESPACE\n"
			"\n"
			"NAMESPACE (RW | RO) (ADD (USER name)? (GROUP name)? (PATH name)? |\n"
			"                         (HOST name)?) |\n"
			"                    (MODIFY (((USER name)? (GROUP name)?\n"
			"                              (PATH name)? (HOST name)?) |"
                        "                             LABEL name))\n"
			"          (SETROOT path | UNSETROOT | DELSETROOT |\n"
			"           ADD alias path | UNADD alias | DELADD alias |\n"
			"           SETLABEL name | UNSETLABEL)*\n"
			"\n"
			"NAMESPACE (RW | RO) MOVE (((USER name)? (GROUP name)?\n"
			"                           (PATH name)? (HOST name)?) |"
                        "                          LABEL name)\n"
			"          (BEFORE | AFTER) (((USER name)? (GROUP name)?\n"
			"                            (PATH name)? (HOST name)?) |"
                        "                           LABEL name)\n"
			);
}

void cli::namespacecmd(std::list<std::string> &args)
{
	connected();

	if (args.empty())
	{
		ns=loadns();
		namespaceshow();
		return;
	}

	std::string rwro=locale->tolower(args.front());
	args.pop_front();

	if (args.empty() || (rwro != "rw" && rwro != "ro"))
		namespace_syntax_error();

	std::string cmd=locale->tolower(args.front());
	args.pop_front();

	if (cmd == "move")
	{
		ns=loadns();

		if (ns.null())
			throw EXCEPTION("No namespaces defined");

		nsmap move, anchor;

		namespaceparseid(args, move);

		if (args.empty())
			namespace_syntax_error();

		std::string verb=
			locale->tolower(args.front());
		args.pop_front();

		if (verb != "before" && verb != "after")
			namespace_syntax_error();

		namespaceparseid(args, anchor);

		STASHER_NAMESPACE::client::base::transaction
			tran=STASHER_NAMESPACE::client::base::transaction::create();

		tran->updobj(NSLISTOBJECT, ns->nsmapuuid,
			     ns->domove(rwro, verb, move, anchor));

		namespaceupdate(tran);
		return;
	}

	if (cmd == "add" || cmd == "modify")
	{
		currentns::adddelinfo adddel;

		adddel.action=cmd;

		namespaceparseid(args, adddel.newnsmap);

		if (adddel.newnsmap.label.size() &&
		    adddel.action != "modify")
			namespace_syntax_error();

		while (!args.empty())
		{
			std::string verb=
				locale->tolower(args.front());
			args.pop_front();

			if (verb == "unsetroot")
			{
				adddel.adds.erase("");
				adddel.dels.erase("");
				adddel.unmaps.insert("");
				continue;
			}

			if (verb == "unsetlabel")
			{
				adddel.unsetlabel=true;
				continue;
			}

			if (verb == "delsetroot")
			{
				adddel.adds.erase("");
				adddel.dels.insert("");
				adddel.unmaps.erase("");
				continue;
			}

			if (args.empty())
				namespace_syntax_error();

			std::string path = toutf8(args.front());
			args.pop_front();

			if (verb == "setlabel")
			{
				adddel.setlabel=path;
				continue;
			}

			if (verb == "setroot")
			{
				if (path.size())
					validate_object_name(path);

				adddel.adds[""]=path;
				adddel.dels.erase("");
				adddel.unmaps.erase("");
				continue;
			}

			if (path.size() == 0)
				namespace_syntax_error();

			if (verb == "unadd")
			{
				if (path.size())
					validate_object_name(path);

				adddel.adds.erase(path);
				adddel.unmaps.insert(path);
				adddel.dels.erase(path);
				continue;
			}

			if (verb == "deladd")
			{
				adddel.adds.erase(path);
				adddel.unmaps.erase(path);
				adddel.dels.insert(path);
				continue;
			}

			if (verb != "add")
				namespace_syntax_error();

			if (args.empty())
				namespace_syntax_error();

			std::string mapping=toutf8(args.front());
			args.pop_front();

			if (mapping.size() > 0)
				validate_object_name(path);

			if (path.find('/') != path.npos)
				throw EXCEPTION(path +
						": cannot be hierarchical");

			adddel.adds[path]=mapping;
			adddel.unmaps.erase(path);
			adddel.dels.erase(path);
		}

		STASHER_NAMESPACE::client::base::transaction
			tran=STASHER_NAMESPACE::client::base::transaction::create();

		ns=loadns();

		if (ns.null())
		{
			ns=x::ptr<cli::currentns>::create(x::uuid());

			tran->newobj(NSLISTOBJECT,
				     ns->doadddel(rwro, adddel));
		}
		else
		{
			tran->updobj(NSLISTOBJECT, ns->nsmapuuid,
				     ns->doadddel(rwro, adddel));
		}
		namespaceupdate(tran);
		return;
	}
	namespace_syntax_error();
}

void cli::namespaceupdate(const STASHER_NAMESPACE::client::base::transaction &tran)
{
	STASHER_NAMESPACE::putresults results=client->put(tran);

	if (results->status != STASHER_NAMESPACE::req_processed_stat)
	{
		ns=loadns();
		throw EXCEPTION(x::to_string(results->status));
	}

	ns->nsmapuuid=results->newuuid;

	namespaceshow();
}

void cli::namespaceparseid(std::list<std::string> &args,
			   nsmap &nsid)
{
	while (!args.empty())
	{
		std::string verb=locale->tolower(args.front());

		std::string *argptr;

		if (verb == "user")
		{
			argptr=&nsid.username;
		}
		else if (verb == "group")
		{
			argptr=&nsid.groupname;
		}
		else if (verb == "path")
		{
			argptr=&nsid.path;
		}
		else if (verb == "host")
		{
			argptr=&nsid.host;
		}
		else if (verb == "label")
		{
			argptr=&nsid.label;
		}
		else break;

		args.pop_front();

		if (args.empty())
			namespace_syntax_error();
		*argptr=args.front();
		args.pop_front();
	}

	if (nsid.label.size() &&
	    (nsid.username.size() ||
	     nsid.groupname.size() ||
	     nsid.path.size() ||
	     nsid.host.size()))
		namespace_syntax_error();
}

x::fd cli::currentns::doadddel(const std::string &rwro,
			       const adddelinfo &info)
{
	std::vector<nsmap> &mapptr=rwro == "rw" ? map.rw:map.ro;

	std::vector<nsmap>::iterator p;

	if (info.action == "add")
	{
		mapptr.push_back(info.newnsmap);
		p= --mapptr.end();
	}
	else
	{
		p=std::find(mapptr.begin(), mapptr.end(), info.newnsmap);

		if (p == mapptr.end())
			throw EXCEPTION("Unable to find a matching mapping");
	}

	for (std::set<std::string>::const_iterator
		     b=info.dels.begin(), e=info.dels.end(); b != e; ++b)
	{
		p->map.erase(*b);
		p->unmap.erase(*b);
	}

	for (std::map<std::string, std::string>::const_iterator
		     b=info.adds.begin(),
		     e=info.adds.end(); b != e; ++b)
	{
		p->map[b->first]=b->second;
		p->unmap.erase(b->first);
	}

	for (std::set<std::string>::const_iterator
		     b=info.unmaps.begin(), e=info.unmaps.end(); b != e; ++b)
	{
		p->map.erase(*b);
		p->unmap.insert(*b);
	}

	if (info.unsetlabel)
		p->label="";

	if (p->map.empty() && p->unmap.empty())
		mapptr.erase(p);
	else if (info.setlabel.size() > 0)
	{
		p->label=info.setlabel;

		if (std::count(mapptr.begin(), mapptr.end(), *p) > 1)
			throw EXCEPTION("Another mapping with the same name already exists");
	}

	return tofd();
}

x::fd cli::currentns::domove(const std::string &rwro,
			     const std::string &beforeafter,
			     const nsmap &move,
			     const nsmap &anchor)
{
	std::vector<nsmap> &mapptr=rwro == "rw" ? map.rw:map.ro;
	std::vector<nsmap>::iterator move_p, anchor_p;

	if ((move_p=std::find(mapptr.begin(), mapptr.end(), move)) ==
	    mapptr.end() ||
	    (anchor_p=std::find(mapptr.begin(), mapptr.end(), anchor)) ==
	    mapptr.end())
		throw EXCEPTION("Unable to find a matching mapping");

	size_t move_i=move_p - mapptr.begin();
	size_t anchor_i=anchor_p - mapptr.begin();

	if (move_i == anchor_i)
		throw EXCEPTION("Why do you want to move a namespace before or after itself?");

	mapptr.insert(beforeafter == "before" ? anchor_p:(anchor_p+1), *move_p);

	if (anchor_i < move_i)
		++move_i;

	mapptr.erase(mapptr.begin()+move_i);
	return tofd();
}

x::fd cli::currentns::tofd()
{
	x::fd serialized(x::fd::base::tmpfile());

	x::fd::base::outputiter iter(serialized);

	x::serialize::iterator<x::fd::base::outputiter> ser_iter(iter);

	ser_iter(map);

	ser_iter.getiter().flush();

	return serialized;
}

x::ptr<cli::currentns> cli::loadns()
{
	x::ptr<currentns> nsptr;

	STASHER_NAMESPACE::client::base::getreq req
		=STASHER_NAMESPACE::client::base::getreq::create();

	req->objects.insert(NSLISTOBJECT);
	req->openobjects=true;

	STASHER_NAMESPACE::contents contents=client->get(req)->objects;

	if (!contents->succeeded)
		throw EXCEPTION(contents->errmsg);

	STASHER_NAMESPACE::contents::base::map_t::iterator
		b=contents->find(NSLISTOBJECT);

	if (b != contents->end())
	{
		nsptr=x::ptr<currentns>::create(b->second.uuid);

		x::fd::base::inputiter beg_iter(b->second.fd), end_iter;
		x::deserialize::iterator<x::fd::base::inputiter>
			deserialize_iter(beg_iter, end_iter);

		deserialize_iter(nsptr->map);
	}

	return nsptr;
}

void cli::namespaceshow()
{
	if (!ns.null())
	{
		const char * const modes[2]={"RO",
					     "RW"};
		const std::vector<nsmap> * const nsmaps[2]=
			{&ns->map.ro, &ns->map.rw};

		bool first=true;
		for (size_t i=0; i<2; ++i)
		{
			for (std::vector<nsmap>::const_iterator
				     b=nsmaps[i]->begin(),
				     e=nsmaps[i]->end(); b != e; ++b)
			{
				if (!first)
					std::cout << std::endl;
				first=false;

				std::cout << "NAMESPACE "
					  << modes[i] << " ADD";
				namespaceshowmapentry(*b);
			}
		}
		std::cout << std::endl;

		if (!first)
			return;
	}

	std::cout << "No namespace mappings defined" << std::endl;
}

void cli::namespaceshowmapentry(const nsmap &b)
{
	if (b.username.size())
		std::cout << " USER " << quote(b.username);

	if (b.groupname.size())
		std::cout << " GROUP " << quote(b.groupname);

	if (b.path.size())
		std::cout << " PATH " << quote(b.path);

	if (b.host.size())
		std::cout << " HOST " << quote(b.host);

	if (b.label.size())
		std::cout << " SETLABEL " << quote(b.label);

	for (std::map<std::string, std::string>::const_iterator
		     mb=b.map.begin(), me=b.map.end();
	     mb != me; ++mb)
	{
		std::cout << " \\" << std::endl << "    ";

		if (mb->first.size() == 0)
		{
			std::cout << "SETROOT "
				  << fromutf8(quote(mb->second));
			continue;
		}

		std::cout << "ADD " << fromutf8(quote(mb->first))
			  << " " << fromutf8(quote(mb->second));
	}

	for (std::set<std::string>::const_iterator
		     ub=b.unmap.begin(), ue=b.unmap.end();
	     ub != ue; ++ub)
	{
		std::cout << " \\" << std::endl << "    ";

		if (ub->size() == 0)
		{
			std::cout << "UNSETROOT";
			continue;
		}

		std::cout << "UNADD " << fromutf8(quote(*ub));
	}
}

void cli::validate_object_name(const std::string &name)
{
	try {
		STASHER_NAMESPACE::
			encoded_object_name_length(name.begin(),
						   name.end());
	} catch (...) {
		throw EXCEPTION(name + " is not a valid object name");
	}
}

int main(int argc, char **argv)
{
	int rc=0;

	signal(SIGCHLD, SIG_DFL);
	try {
		objclientopts opts;

		std::list<std::string> args=opts.parse(argc, argv)->args;

		cli parser;

		if (opts.alarm->value)
			alarm(opts.alarm->value);
		if (opts.connect->is_set())
			parser.connect(opts.connect->value);
		if (opts.admin->is_set())
			parser.admin(opts.admin->value);

		if (!args.empty())
		{
			parser.command(args);
		}
		else
		{
			if (parser.interactive)
				std::cout << "Ready, EOF to exit." << std::endl;

			std::string line;
			std::string acc_line;
			bool continue_prompt=false;

			while (parser.getline(line, continue_prompt))
			{
				if (line.size() && *--line.end() == '\\')
				{
					acc_line +=
						line.substr(0, line.size()-1);
					continue_prompt=true;
					continue;
				}

				acc_line += line;

#if HAVE_READLINE_READLINE_H

				if (acc_line.size() > 0)
					add_history(acc_line.c_str());
#endif

				parser.command(acc_line);
				acc_line="";
				continue_prompt=false;
			}
			if (parser.interactive)
				std::cout << std::endl << std::flush;
		}
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		rc=1;
	}

	exit(rc);
	return (0);
}

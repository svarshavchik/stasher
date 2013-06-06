/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

#include "objrepo_config.h"
#include "repomg.H"
#include <x/strftime.H>
#include <x/locale.H>
#include <x/property_list.H>
#include <x/property_save.H>
#include <x/messages.H>
#include <x/join.H>
#include <sys/time.h>
#include <iostream>
#include <sstream>
#include <iomanip>

class repomgcliopts_base {

public:
	x::const_locale l;
	x::wctype wc;

	std::wstring keystrengths;

	std::wstring defaultkeystrength;

	std::wstring digests;

	std::wstring quoted_list(std::vector<std::wstring> &list)
	{
		std::transform(list.begin(),
			       list.end(),
			       list.begin(),
			       [&](const std::wstring &w)
			       {
				       return L"\""
					       + wc.tolower(w)
					       + L"\"";
			       });
		return x::join(list, L", ");
	}

	repomgcliopts_base(const x::wmessagesptr &msgcat)
		: l(msgcat->getLocale()),
		  wc(l),
		  keystrengths(
			       ({
				       std::vector<std::wstring> list;

				       x::gnutls::sec_param::enumerate(list, l);

				       quoted_list(list);
			       })
			       ),

		  defaultkeystrength
		  (L"\""
		   + wc.tolower(x::towstring(x::gnutls::sec_param
					     (GNUTLS_SEC_PARAM_NORMAL), l))
		   + L"\""),

		  digests(
			  ({
				  std::vector<std::wstring> list;

				  x::gnutls::digest_algorithm::enumerate(list,
									 l);

				  quoted_list(list);
			  })
			  )
	{
	}
};

#include "repomgcliopts.h"
#include "localstatedir.h"

x::property::value<std::string> default_clusterdir(L"objrepo::clusterdir",
						   localstatedir "/stasher/clusters");

x::property::value<std::string> default_newnodedir(L"objrepo::newnodedir",
						   localstatedir "/stasher/newnodes");

class display_keys : public repomg::key_list_callback {

	x::locale l;

	static const int key_width=16;
	static const int name_width=20;
	static const int date_width=4+1+2+1+2+1+2+1+2+1+2;

	void row(const std::string &keyfilename,
		 const std::string &clustername,
		 const std::string &activation_time,
		 const std::string &expiration_time) const;

public:
	display_keys();

	void operator()(const std::string &keyfilename,
			const std::string &clustername,
			time_t activation_time,
			time_t expiration_time) const;
};

display_keys::display_keys() : l(x::locale::create(""))
{
	row("Key ID", "Cluster name",
	    ("Activation" + std::string(date_width, ' ')).substr(0, date_width),
	    "Expiration");

	row(std::string(key_width, '-'),
	    std::string(name_width, '-'),
	    std::string(date_width, '-'),
	    std::string(date_width, '-'));
}

void display_keys::operator()(const std::string &keyfilename,
			      const std::string &clustername,
			      time_t activation_time,
			      time_t expiration_time) const
{
	row(keyfilename, clustername,
	    x::strftime(x::tzfile::base::local(), l)(activation_time)("%F %T"),
	    x::strftime(x::tzfile::base::local(), l)(expiration_time)("%F %T"));
}

void display_keys::row(const std::string &keyfilename,
		       const std::string &clustername,
		       const std::string &activation_time,
		       const std::string &expiration_time) const
{
	std::cout << std::setw(key_width) << std::left << keyfilename << ' '
		  << std::setw(name_width) << std::left << clustername << ' '
		  << std::setw(0)
		  << activation_time << "  "
		  << expiration_time << std::endl;
}

static x::property::list mkconfig()
{

	std::ostringstream o;


	x::property::list proplist=
		x::property::list::create();

	x::locale env=x::locale::create("");

	proplist->load(L"x::logger::handler::objrepo=logs/%Y-%m-%d.log"
		       L"\n"
		       L"x::logger::handler::objrepo::rotate=logs/*.log"
		       L"\n"
		       L"x::logger::handler::objrepo::keep=7"
		       L"\n"
		       L"x::logger::format::objrepo=%Y-%m-%d %H:%M:%S"
		       L" @{level}: @{msg}"
		       L"\n"
		       L"x::logger::log::handler::default=objrepo"
		       L"\n"
		       L"x::logger::log::handler::default::format=objrepo"
		       L"\n"
		       L"x::logger::log::level=info"
		       L"\n"
		       L"x::@log::level=error"
		       L"\n", true, true,
		       x::property::errhandler::errthrow(), env);

	return proplist;
}



static int clustmg(int argc, char **argv)
{
	x::locale locale(x::locale::create(""));
	x::wmessages msgcat(x::wmessages::create(locale, "stasher"));
	repomgcliopts opts(msgcat);
	std::list<std::string> args(opts.parse(argc, argv, locale)->args);

#define DAYS(v)	((v).years * 365 +			\
		 (v).years / 4 +			\
		 (v).months * 30 +			\
		 (v).months / 2	+			\
		 (v).weeks * 7 +			\
		 (v).days)

	if (opts.clustkey->isSet())
	{
		if (opts.clustkey_generate->isSet())
		{
			time_t days=DAYS(opts.clustkey_expire->value);

			if (days < 0 || days > 10 * 365 + 10/4)
			{
				std::cerr << "Key's duration must be 0-10 years"
					  << std::endl;
				return 1;
			}
			days += 10;

			std::string key_type=opts.clustkey_type->value;

			time_t activation_time=time(NULL);
			time_t expiration_time=activation_time +
				days * 24 * 60 * 60;

			std::string dirname;

			if (args.empty())
			{
				dirname = default_clusterdir.getValue() + "/"
					+ opts.clustkey_name->value;
			}
			else
			{
				dirname=args.front();
				args.pop_front();
			}

			repomg::clustkey_generate(dirname,
						  opts.clustkey_name->value,
						  activation_time,
						  expiration_time,
						  key_type,
						  opts.clustkey_bits->value,
						  opts.clustkey_digest->value);
			return 0;
		}

		if (args.empty())
		{
			std::cerr << "Missing key directory name"
				  << std::endl;
			return 1;
		}

		std::string dirname=args.front();
		args.pop_front();

		if (opts.clustkey_remove->isSet())
		{
			if (args.empty())
			{
				std::cerr << "Missing key name/id"
					  << std::endl;
				return 1;
			}

			repomg::clustkey_remove(dirname, args.front());
			return 0;
		}
		else if (opts.clustkey_list->isSet())
		{
			repomg::cluster_key_list(dirname, display_keys());
			return 0;
		}
		else if (opts.clustkey_export->isSet())
		{
			if (args.empty())
			{
				std::cerr << "Missing node directory name"
					  << std::endl;
				return 1;
			}

			repomg::key_export(dirname, args.front());
		}
	}

	if (opts.nodekey->isSet())
	{
		if (args.empty())
		{
			std::cerr << "Missing key directory name"
				  << std::endl;
			return 1;
		}

		std::string nodekeydir=args.front();
		args.pop_front();

		if (opts.nodekey_generate->isSet())
		{
			std::string clustkeydir;

			if (args.empty())
			{
				if (opts.nodekey_name->value.size() == 0)
				{
					std::cerr << "Missing cluster key directory name"
						  << std::endl;
					return 1;
				}

				clustkeydir=nodekeydir;
				nodekeydir=default_newnodedir.getValue()
					+ "/" + opts.nodekey_name->value;
			}
			else
			{
				clustkeydir=args.front();
				args.pop_front();
			}

			{
				std::string stdclustkeydir =
					default_clusterdir.getValue() + "/"
					+ clustkeydir;

				if (clustkeydir.find('/') ==
				    std::string::npos &&
				    access(stdclustkeydir.c_str(), 0) == 0)
				{
					clustkeydir=stdclustkeydir;
				}
			}

			long days=DAYS(opts.nodekey_expire->value);

			if (days < 0 || days > 5 * 365 + 5 / 4)
			{
				std::cerr << "Key's duration must be 0-5 years"
					  << std::endl;
				return 1;
			}

			days += 10;

			time_t activation_time=time(NULL);
			time_t expiration_time=activation_time +
				days * (24L * 60 * 60);

			bool firsttime=access(nodekeydir.c_str(), 0) < 0;

			repomg::nodekey_generate(nodekeydir,
						 clustkeydir,
						 opts.nodekey_signkey->value,
						 opts.nodekey_name->value,
						 activation_time,
						 expiration_time,
						 opts.nodekey_bits->value,
						 opts.nodekey_digest->value);

			if (firsttime)
			{
				std::string logdir=nodekeydir + "/logs";
				std::string datadir=nodekeydir + "/data";

				mkdir(logdir.c_str(), 0777);
				mkdir(datadir.c_str(), 0777);

				x::fd propfile=x::fd::create(nodekeydir +
							     "/properties",
							     0644);

				std::map<x::property::propvalue,
					 x::property::propvalue> propmap;

				mkconfig()->enumerate(propmap);

				x::ostream w=propfile->getostream();

				x::locale env=x::locale::create("");

				x::property::save_properties<char>
					(propmap,
					 std::ostreambuf_iterator<char>(*w),
					 env);
				*w << std::flush;
				propfile->close();
			}
		}
	}

	if (opts.certreload->value)
	{
		if (args.empty())
		{
			std::cerr << "Missing node directory name"
				  << std::endl;
			return 1;
		}

		std::string nodekeydir=args.front();
		args.pop_front();

		repomg::nodekey_certreload(nodekeydir);
	}
	return 0;
}

int main(int argc, char **argv)
{
	int rc;

	try {
		rc=clustmg(argc, argv);
	} catch (const x::exception &e)
	{
		std::cerr << e << std::endl;
		exit(1);
	}

	exit(rc);
	return (0);
}

/*
** Copyright 2012 Double Precision, Inc.
** See COPYING for distribution information.
*/

static x::property::value<bool> noalarm(L"noalarm", false);

{
#ifndef ENABLE_NEWNODERECONNECT
	x::property::load_property(L"connection::debugnonewconnect",
				   L"true", true, true,
				   x::property::errhandler::errlog(),
				   x::locale::create("C"));
#endif
	x::option::list opts(x::option::list::create());

	opts->addDefaultOptions();

	x::option::parser opts_parser(x::option::parser::create());

	opts_parser->setOptions(opts);

	int err=opts_parser->parseArgv(argc, argv);

	if (err == 0)
		err=opts_parser->validate();

	if (err)
	{
		if (err == x::option::parser::base::err_builtin)
			exit(0);

		std::cout << opts_parser->errmessage() << std::endl;
		exit(1);
	}

#define ALARM(n) do {				\
		if (!noalarm.getValue())	\
			alarm(n);		\
	} while(0)

	ALARM(300);
}

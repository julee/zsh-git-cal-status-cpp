#ifndef TEST_OPTIONS_HEAD
#define TEST_OPTIONS_HEAD

#include <boost/program_options.hpp>
#include <iostream>

namespace po = boost::program_options;

struct Options
{
	boost::program_options::options_description options;
	po::variables_map vm_local;

	std::string	test;
	int		num;
	bool		all;
	bool		real;

	Options(int argc, char** argv, unsigned columns, unsigned max_description_length)
	// to make it work put in `int main(……)`:
	// #include <sys/ioctl.h>
	// struct winsize w; ioctl(0, TIOCGWINSZ, &w); Options opt(argc,argv,w.ws_col,15);
		: options(columns		// width of the terminal
		, max_description_length)	// if option description eg. `--diff_max arg (=0.34999999999999998)` is longer than this, then explanation starts at next line
	{
		options.add_options()
		("help,h"       ,"display this help.")
		("test"         ,po::value<std::string>(&test         )->default_value(""),"używam: fullDebugSendRequestWithApiKey")
		("num"          ,po::value<int        >(&num          )->default_value(1 ),"test number")
		("all"          ,po::bool_switch       (&all          )->default_value(false),"Run all tests, excluding real orders.")
		("real"         ,po::bool_switch       (&real         )->default_value(false),"Also enable running real orders.")
		;

		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, options), vm);
		po::notify(vm);

		if (vm.count("help")) {
			std::cout << options << "\n";
			exit(0);
		}
		vm_local=vm;	
	};

	void print(std::ostream& os=std::cout,std::string prefix="") const
	{
		os << prefix << "Settings: \n";
		for (const auto& it : vm_local) {
			os << prefix << "  " << it.first.c_str() << "   \t= ";
			auto& value = it.second.value();
			if	(auto v = boost::any_cast<double>(&value))	os << *v;
			else if (auto v = boost::any_cast<int>(&value))		os << *v;
			else if (auto v = boost::any_cast<size_t>(&value))	os << *v;
			else if (auto v = boost::any_cast<bool>(&value))	os << (*v)?std::string("true"):std::string("false");
			else if (auto v = boost::any_cast<std::string>(&value))	os << *v;
			else	os << prefix << "error";
			os << prefix << "\n";
		}
		os << prefix << "\n";
	};
	
};

#endif


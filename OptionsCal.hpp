#ifndef TEST_OPTIONS_HEAD
#define TEST_OPTIONS_HEAD

#include <boost/program_options.hpp>
#include <iostream>

namespace po = boost::program_options;

struct OptionsCal
{
	boost::program_options::options_description options;
	po::variables_map vm_local;

	std::string	git_dir;
	std::string	work_tree;
	std::string	author;
	bool		start_with_sunday;
	bool		number_days;
	bool		number_commits;
	int		print_authors;
	bool		include_emails;
	bool		print_streaks;
	bool		only_last_year;
	bool		use_calendar_years;

	OptionsCal(int argc, char** argv, unsigned columns, unsigned max_description_length)
	// to make it work put in `int main(……)`:
	// #include <sys/ioctl.h>
	// struct winsize w; ioctl(0, TIOCGWINSZ, &w); OptionsCal opt(argc,argv,w.ws_col,15);
		: options(columns		// width of the terminal
		, max_description_length)	// if option description eg. `--diff_max arg (=0.34999999999999998)` is longer than this, then explanation starts at next line
	{
		options.add_options()
		("help,h"                 , "display this help.")
		("git-dir"                , po::value<std::string>(&git_dir                )->default_value(""   ),"The --git-dir for git")
		("work-tree"              , po::value<std::string>(&work_tree              )->default_value(""   ),"The --work-tree for git")
		("author,a"               , po::value<std::string>(&author                 )->default_value(""   ),"analyse commits of only one selected author, otherwise all authors are included")
		("only-last-year,y"       , po::bool_switch       (&only_last_year         )->default_value(false),"print only single year of data, skip older data")
		("use-calendar-years,Y"   , po::bool_switch       (&use_calendar_years     )->default_value(false),"the calendar will be organized using `calandar years`, not `days back`")
		("start-with-sunday,s"    , po::bool_switch       (&start_with_sunday      )->default_value(false),"start week with sunday instead of monday")
		("number-days,n"          , po::bool_switch       (&number_days            )->default_value(false),"instead of ◼ put the day of the month (as in real calendar)")
//		(",d"                     , po::bool_switch       (&number_days            )->default_value(false),"same as --number-days, -n")
		("number-commits,c"       , po::bool_switch       (&number_commits         )->default_value(false),"instead of ◼ put the commit count number")
		("print-authors,N"        , po::value<int>        (&print_authors          )->default_value(0    ),"print the commit count and streaks per author for top N authors")
//		(",A"                     , po::value<int>        (&print_authors          )->default_value(0    ),"same as --print-authors, -N")
		("print-streaks,S"        , po::bool_switch       (&print_streaks          )->default_value(false),"print the commit count and streaks for all authors merged together (or single author if -a is specified)")
		("include-emails,e"       , po::bool_switch       (&include_emails         )->default_value(false),"also print the author's email")
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

	std::string rebuild() const {
		std::string args{};
		for (const auto& it : vm_local) {
			//args+="--" + it.first + " ";
			auto& value = it.second.value();
			     if (auto v2 = boost::any_cast<int        >(&value))	args += " --"+it.first+" "+ boost::lexical_cast<std::string>( *v2 );
			else if (auto v5 = boost::any_cast<std::string>(&value))	args += (*v5!="")?std::string(" --"+it.first+" "+ boost::lexical_cast<std::string>( *v5 )):std::string({});
			else if (auto v4 = boost::any_cast<bool       >(&value))	args +=                    (*v4)?std::string(" --must-update-now"):std::string("");
		}
		return args;
	}

	void print(std::ostream& os=std::cout,std::string prefix="") const
	{
		os << prefix << "Settings: \n";
		for (const auto& it : vm_local) {
			os << prefix << "  " << it.first.c_str() << "   \t= ";
			auto& value = it.second.value();
			if	(auto v1 = boost::any_cast<double>(&value))		os << *v1;
			else if (auto v2 = boost::any_cast<int>(&value))		os << *v2;
			else if (auto v3 = boost::any_cast<size_t>(&value))		os << *v3;
			else if (auto v4 = boost::any_cast<bool>(&value))		os <<(*v4)?std::string("true"):std::string("false");
			else if (auto v5 = boost::any_cast<std::string>(&value))	os << *v5;
			else	os << prefix << "error";
			os << prefix << "\n";
		}
		os << prefix << "\n";
	};
};

#endif


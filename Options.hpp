#ifndef TEST_OPTIONS_HEAD
#define TEST_OPTIONS_HEAD

#include <boost/program_options.hpp>
#include <iostream>

namespace po = boost::program_options;

struct Options
{
	boost::program_options::options_description options;
	po::variables_map vm_local;

	std::string	pwd_dir;
	std::string	git_dir;
	std::string	work_tree;
	std::string	branch_master_override;
	std::string	whoami;
	std::string	author;
	int		refresh_sec;
	bool		must_update_now;
	bool		start_with_sunday;
	bool		number_days;
	//int		num;
	//bool		flag;

	Options(int argc, char** argv/*, unsigned columns, unsigned max_description_length*/)
	// to make it work put in `int main(……)`:
	// #include <sys/ioctl.h>
	// struct winsize w; ioctl(0, TIOCGWINSZ, &w); Options opt(argc,argv,w.ws_col,15);
		//: options(columns		// width of the terminal
		//, max_description_length)	// if option description eg. `--diff_max arg (=0.34999999999999998)` is longer than this, then explanation starts at next line
	{
		options.add_options()
		("help,h"                 , "display this help.")
		("pwd-dir"                , po::value<std::string>(&pwd_dir                )->default_value(""   ),"current pwd directory")
		("git-dir"                , po::value<std::string>(&git_dir                )->default_value(""   ),"The --git-dir for git")
		("work-tree"              , po::value<std::string>(&work_tree              )->default_value(""   ),"The --work-tree for git")
		("branch-master-override" , po::value<std::string>(&branch_master_override )->default_value(""   ),"If the branch name is \"master\" then override it to given name")
		("whoami"                 , po::value<std::string>(&whoami                 )->default_value(""   ),"output of whoami")
		("refresh-sec"            , po::value<int        >(&refresh_sec            )->default_value(10   ),"time in seconds between a forced refresh")
		("must-update-now"        , po::bool_switch       (&must_update_now        )->default_value(false),"If set, will force git updating right now.")
		//("num"                  , po::value<int        >(&num                    )->default_value(1    ),"test number")
		//("flag"                 , po::bool_switch       (&flag                   )->default_value(false),"test flag.")
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


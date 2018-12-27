#include "Options.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>

// FIXME - duplikat z moj-git-status.cpp, bom leniwy
std::string exec(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    auto pipe = popen(cmd.c_str(), "r"); // get rid of shared_ptr
    if (!pipe) { std::cerr << __PRETTY_FUNCTION__ << "pipe error" << "\n";exit(1); };
    while (!feof(pipe)) {
        if (fgets(buffer.data(), 128, pipe) != nullptr)
            result += buffer.data();
    }
    auto rc = pclose(pipe);
    if (rc == EXIT_SUCCESS) { // == 0
	return result;
    } else {
	std::cerr << __PRETTY_FUNCTION__ << "error: " << rc << "\n";
	exit(1);
    }
}

int main(int argc, char** argv)
{
	Options opt(argc,argv);

	std::string gdir  = ""; if(opt.git_dir   != "") gdir  = " --git-dir "  +opt.git_dir;
	std::string wtree = ""; if(opt.work_tree != "") wtree = " --work-tree "+opt.work_tree;
	std::string author= ""; if(opt.author    != "") author= " --author "   +opt.author;
	std::string call  = "/usr/bin/git "+gdir+wtree+author;

	boost::posix_time::ptime date1970(boost::gregorian::date(1970,1,1));
	auto        now  = boost::posix_time::second_clock::universal_time();
	int         yy   = (now - date1970).hours()/24/365 - 5;
	//std::cout << "years since 1975 = " << yy << "\n";

	std::string git_cal_result = exec(call+" log --no-merges --pretty=format:\"%at\" --since=\""+(boost::lexical_cast<std::string>(yy))+" years\"");

	size_t newlines = std::count(git_cal_result.begin(), git_cal_result.end(), '\n');
	std::vector<boost::posix_time::ptime> commits;
	commits.reserve(newlines+10);
	std::string tmp;
	std::stringstream ss(git_cal_result);
	while(std::getline(ss,tmp,'\n')){
		commits.push_back( boost::posix_time::from_time_t( boost::lexical_cast<time_t>( tmp ) ) );
	}

//	std::cout << git_cal_result << "\n" << "\n" << newlines << "\n";
	for(auto& a: commits) {
		std::cout << a << " " << a.date().year() << " " << a.date().month()  << " " << a.date().day() << "\n";
	}


}

#include "Options.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <algorithm>

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

struct Dot {
	int q1{0},q2{0},q3{0};
	static constexpr int colors[]={ 237, 139, 40, 190, 1 };
	Dot(const std::vector<int>& cc) {
		std::vector<int> vv{};
		for(const auto& a:cc) { if(a!=0) vv.push_back(a); }
		std::sort(vv.begin(),vv.end());
		size_t s = vv.size();
		q1=vv[s/4];
		q2=vv[s/2];
		q3=vv[3*s/4];
		std::cerr << vv[0] << " " << q1 << " " << q2 << " " << q3 << "\n";
	};
	void print(int val) {
		int index=   val == 0  ? 0
			   : val <= q1 ? 1
			   : val <= q2 ? 2
			   : val <= q3 ? 3
				       : 4;
		std::cout << val << "\e[38;5;"<<(boost::lexical_cast<std::string>(colors[index]))<<"m◼ \e[0m";
	}
};

int main(int argc, char** argv)
{
	Options opt(argc,argv);

	std::string gdir  = ""; if(opt.git_dir   != "") gdir  = " --git-dir "  +opt.git_dir;
	std::string wtree = ""; if(opt.work_tree != "") wtree = " --work-tree "+opt.work_tree;
	std::string author= ""; if(opt.author    != "") author= " --author "   +opt.author;
	std::string call  = "/usr/bin/git "+gdir+wtree+author;

	boost::posix_time::ptime date1970(boost::gregorian::date(1970,1,1));
	auto        now        = boost::posix_time::second_clock::universal_time();
	boost::posix_time::ptime now_date_UTC{now.date()};
//	auto        now_local  = boost::posix_time::second_clock::local_time();
//	auto        UTC_local  = now-now_local;
	int         yy         = (now - date1970).hours()/24/365 - 5;
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

	std::sort(commits.rbegin(),commits.rend()); // w sumie to już jest posortowane, to tak tylko dla pewności

	auto start = *commits.rbegin();

	int  days  = (now-start).hours()/24 + 10;
	std::vector<int> count_per_day(days , 0);

//	std::cout << git_cal_result << "\n" << "\n" << newlines << "\n";
	for(auto& that_commit_UTC : commits) {
//		std::cout << a << " " << a.date().year() << " " << a.date().month()  << " " << a.date().day() << "\n";
		std::cout << that_commit_UTC << "\n";
		count_per_day[ ( now_date_UTC - boost::posix_time::ptime(that_commit_UTC.date()) /* + UTC_local */ ).hours()/24 ] += 1;
	}


	// no to mam teraz licznik ile było każdego dnia, oraz max.
	Dot dot(count_per_day);
	for(auto& a : count_per_day) {
		dot.print(a);
		std::cout << "\n";
	}
}

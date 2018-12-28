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
//	std::cout << s << " ← size\n";
		q1=vv[s/4];
		q2=vv[s/2];
		q3=vv[3*s/4];
//		std::cerr << vv[0] << " " << q1 << " " << q2 << " " << q3 << "\n";
	};
	void print(int val) {
		int index=   val == 0  ? 0
			   : val <= q1 ? 1
			   : val <= q2 ? 2
			   : val <= q3 ? 3
				       : 4;
		std::cout <</* val <<*/ "\e[38;5;"<<(boost::lexical_cast<std::string>(colors[index]))<<"m◼ \e[0m";
	}
};

int main(int argc, char** argv)
{
	Options opt(argc,argv);
//opt.print();
	std::string gdir  = ""; if(opt.git_dir   != "") gdir  = " --git-dir "  +opt.git_dir;
	std::string wtree = ""; if(opt.work_tree != "") wtree = " --work-tree "+opt.work_tree;
	std::string author= ""; if(opt.author    != "") author= " --author='"   +opt.author+"'";
	std::string call  = "/usr/bin/git "+gdir+wtree;

	boost::posix_time::ptime date1970(boost::gregorian::date(1970,1,1));
	auto        now            = boost::posix_time::second_clock::universal_time();
	auto        now_local_date = boost::posix_time::second_clock::local_time().date();
	boost::posix_time::ptime now_date_UTC{now.date()};
	int         yy             = (now - date1970).hours()/24/365 - 5;

	std::string git_cal_result = exec(call+" log --no-merges --pretty=format:\"%at\" --since=\""+(boost::lexical_cast<std::string>(yy))+" years\""+author);

	size_t newlines = std::count(git_cal_result.begin(), git_cal_result.end(), '\n');
	std::vector<boost::posix_time::ptime> commits;
	commits.reserve(newlines+10);
	std::string tmp;
	std::stringstream ss(git_cal_result);
	while(std::getline(ss,tmp,'\n')) { commits.push_back( boost::posix_time::from_time_t( boost::lexical_cast<time_t>( tmp ) ) ); }

	std::sort(commits.rbegin(),commits.rend()); // w sumie to już jest posortowane, to tak tylko dla pewności
	auto start = *commits.rbegin();

	int WEEKS = 52;
	int days  = (now-start).hours()/24+1;
	int years = std::ceil(double(days)/(WEEKS*7));//(365.25));
	int day_of_week  = now_local_date.day_of_week();
	std::string weeknames=" M W F ";
	if(not opt.start_with_sunday) {
		day_of_week = (day_of_week+6)%7;
		weeknames="M W F S";
	}
	days=years*WEEKS*7 - (6-day_of_week);
	std::vector<int> count_per_day(days , 0);

	for(auto& that_commit_UTC : commits) { count_per_day[ ( now_date_UTC - boost::posix_time::ptime(that_commit_UTC.date()) ).hours()/24 ] += 1; }

	// no to mam teraz licznik ile było każdego dnia. Zaokrąglony do wielokrotności 52-tygodni w górę
	Dot dot(count_per_day);
/*
	std::cout << "today is: " << now_local_date.day_of_week()  << "\n";
	std::cout << "today is: " << now_local_date.month()  << "\n";
	std::cout << "today is: " << now_local_date.year()  << "\n";
*/
	// ↓ year      ↓ week       ↓ day   days_back
	std::vector<std::vector<std::vector<int>>> kalendarz(years,std::vector<std::vector<int>>(WEEKS,std::vector<int>(7,-1)));
	int current_year = now_local_date.year();
	int current_week = WEEKS-1;
	int years_back   = 0;
	for(size_t i = 0 ; i<count_per_day.size() ; ++i) {
		assert(years_back   >=0 and years_back   < years);
		assert(current_week >=0 and current_week < WEEKS);
		assert(day_of_week  >=0 and day_of_week  < 7    );
		kalendarz[years_back][current_week][day_of_week] = i;
		--day_of_week;
		if(day_of_week==-1) {
			day_of_week = 6;
			current_week--;
			if(current_week==-1) {
				years_back++;
				current_week=WEEKS-1;
			}
		}
	}

	for(int y = years-1 ; y>=0 ; --y) {
		std::cout <<         "                                                        "<<current_year-y<<"\n";
		for(int d=0 ; d<7 ; ++d) {
			std::cout << "    "<<weeknames[d]<<" ";
			for(int w=0 ; w<WEEKS ; ++w) {
				int days_back = kalendarz[y][w][d];
				if(days_back != -1) {
					assert(days_back >=0 and days_back<days);
					int val = count_per_day[days_back];
					dot.print(val);
				} else {
					std::cout << "  ";
				}
			}
			std::cout << "\n";
		}
	}

}

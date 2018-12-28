#include "OptionsCal.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <algorithm>
#include <sstream>
#include <codecvt>

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

struct CommitInfo {
	boost::posix_time::ptime time;
	std::wstring author;
};

struct AuthorStreak {
	int count{0};
	boost::gregorian::date start{boost::gregorian::date(1970,1,1)};
	boost::gregorian::date end  {boost::gregorian::date(1970,1,1)};
};

typedef std::map<std::wstring,AuthorStreak> AuthorsCount;

struct DayInfo {
	int count;
	AuthorsCount authors;
};

struct Dot {
	int q1{0},q2{0},q3{0};
	const std::string esc{char{27}}; // zamiast "\e" który generuje warningi.
	const std::vector<int> colors={ 237, 139, 40, 190, 1 };
	Dot(const std::vector<DayInfo>& cc) {
		std::vector<int> vv{};
		for(const auto& a:cc) { if(a.count != 0) vv.push_back(a.count); }
		std::sort(vv.begin(),vv.end());
		size_t s = vv.size();
//	std::cout << s << " ← size\n";
		q1=vv[s/4];
		q2=vv[s/2];
		q3=vv[3*s/4];
//		std::cerr << vv[0] << " " << q1 << " " << q2 << " " << q3 << "\n";
	};
	void print(int val , const boost::posix_time::ptime& then, const OptionsCal& opt) {
		int index=   val == 0  ? 0
			   : val <= q1 ? 1
			   : val <= q2 ? 2
			   : val <= q3 ? 3
				       : 4;
		if(opt.number_days) {
			int d = then.date().day();
			std::string s="";
			if(d<10) s=" ";
			std::cout << colStart(index)<< s << d <<colEnd();
		} else if(opt.number_commits) {
			int d = val;
			std::string s="";
			if(d<10) s=" ";
			std::cout << colStart(index)<< s << d <<colEnd();
		} else {
			put(index);
		}
	};
	void put(int index) {
		std::cout << colStart(index)<<"◼ "<<colEnd();
	};
	std::string colStart(int index) {
		return esc+"[38;5;"+(boost::lexical_cast<std::string>(colors[index]))+"m";
	}
	std::string colEnd() {
		return esc+"[0m";
	}
};

int main(int argc, char** argv)
{
	OptionsCal opt(argc,argv);
//opt.print();
	std::string gdir  = ""; if(opt.git_dir   != "") gdir  = " --git-dir "  +opt.git_dir;
	std::string wtree = ""; if(opt.work_tree != "") wtree = " --work-tree "+opt.work_tree;
	std::string author= ""; if(opt.author    != "") author= " --author='"   +opt.author+"'";
	std::string call  = "/usr/bin/git "+gdir+wtree;
	if(opt.number_commits and opt.number_days) {
		std::cerr << "--number-days,-n  and  --number-commits,-c  are mutually exclusive.\n";
		exit(1);
	}

	boost::posix_time::ptime date1970(boost::gregorian::date(1970,1,1));
	auto        now            = boost::posix_time::second_clock::universal_time();
	auto        now_local_date = boost::posix_time::second_clock::local_time().date();
	boost::posix_time::ptime now_date_UTC{now.date()};
	boost::posix_time::ptime now_date_LOC{now_local_date};
	int         yy             = (now - date1970).hours()/24/365 - 5;

	std::string fmt="";
	if(opt.print_authors != 0) {
		fmt = " %aN";
		if(opt.include_emails) {
			fmt = " %aE, %aN";
		}
	}
	std::string git_cal_result = exec(call+" log --no-merges --pretty=format:\"%at"+fmt+"\" --since=\""+(boost::lexical_cast<std::string>(yy))+" years\""+author);

	size_t longest_author{0};
	size_t newlines = std::count(git_cal_result.begin(), git_cal_result.end(), '\n');
	std::vector<CommitInfo> commits;
	commits.reserve(newlines+10);
	std::string tmp;
	std::stringstream ss(git_cal_result);
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	while(std::getline(ss,tmp,'\n')) {
		std::stringstream tt(tmp);
		time_t time{0};
		tt >> time;
		std::string aut{};
		std::getline(tt,aut,'\n');
		longest_author = std::max( aut.size() , longest_author );
		commits.push_back( { boost::posix_time::from_time_t( boost::lexical_cast<time_t>( time ) ) , converter.from_bytes(aut) } );
	}

	std::sort(commits.rbegin() , commits.rend() , [](const CommitInfo& a, const CommitInfo& b)->bool { return a.time < b.time; } ); // w sumie to już jest posortowane, to tak tylko dla pewności
	auto start = commits.rbegin()->time;

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
	std::vector<DayInfo> count_per_day(days , {0,{}} );

	for(auto& that_commit_UTC : commits) {
		int idx = ( now_date_UTC - boost::posix_time::ptime(that_commit_UTC.time.date()) ).hours()/24;
		count_per_day[ idx ].count += 1;
		count_per_day[ idx ].authors[that_commit_UTC.author].count += 1;
//std::cerr << that_commit_UTC.author << " " << count_per_day[ idx ].authors[that_commit_UTC.author] << "\n";
	}

	// no to mam teraz licznik ile było każdego dnia. Zaokrąglony do wielokrotności 52-tygodni w górę
	Dot dot(count_per_day);
/*
	std::cout << "today is: " << now_local_date.day_of_week()  << "\n";
	std::cout << "today is: " << now_local_date.month()  << "\n";
	std::cout << "today is: " << now_local_date.year()  << "\n";
*/
	// ↓ year      ↓ week       ↓ day   days_back
	std::vector<std::vector<std::vector<int>>> kalendarz(years,std::vector<std::vector<int>>(WEEKS,std::vector<int>(7,-1)));
//	int current_year = now_local_date.year();
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

	ss.str("");
	for(int y = years-1 ; y>=0 ; --y) {
		//std::cout <<         "                                                        "<<current_year-y<<"\n";
		bool just_printed=false;
		bool got_year=false;
		int prev_month=-1;
		AuthorsCount authors_count;
		std::string months_str("      ");//po tych spacjach zaczyna pisać nazwy miesięcy
		std::string year_str(months_str);
		for(int w=0 ; w<WEEKS ; ++w) {
			int days_back = kalendarz[y][w][0];
			if(days_back != -1) {
				boost::posix_time::ptime then = now_date_LOC - boost::posix_time::time_duration(boost::posix_time::hours(24*days_back));
				int new_month = then.date().month();
				if( (not got_year) and year_str.size() >= 56 ) {
					year_str.resize(56);
					year_str += boost::lexical_cast<std::string>(then.date().year());
					got_year=true;
				}
				if((new_month != prev_month) and (not just_printed)) {
					ss << then.date().month();
					months_str += ss.str(); ss.str("");
					if(not got_year) year_str   += "   ";
					just_printed=true;
					prev_month=new_month;
				} else {
					if(just_printed) {
						months_str += " ";
						if(not got_year) year_str   += " ";
						just_printed=false;
					} else {
						months_str += "  ";
						if(not got_year) year_str   += "  ";
					}
				}
			} else {
				months_str += "  ";
				if(not got_year) year_str   += "  ";
			}
		}
		std::cout << year_str << "\n" << months_str << "\n";

		for(int d=0 ; d<7 ; ++d) {
			std::cout << "    "<<weeknames[d]<<" ";
			for(int w=0 ; w<WEEKS ; ++w) {
				int days_back = kalendarz[y][w][d];
				if(days_back != -1) {
					assert(days_back >=0 and days_back<days);
					int val = count_per_day[days_back].count;
					for(const auto& aa : count_per_day[days_back].authors) {
						authors_count[aa.first].count += aa.second.count;
					}
					boost::posix_time::ptime then = now_date_LOC - boost::posix_time::time_duration(boost::posix_time::hours(24*days_back));
					dot.print(val,then,opt);
				} else {
					std::cout << "  ";
				}
			}
			std::cout << "\n";
		}
		if(opt.print_authors != 0 or opt.print_streaks)
		{
			std::vector<std::pair<std::wstring,AuthorStreak> > authors_count_sorted{};
			for(const auto& aa : authors_count) {
				authors_count_sorted.push_back({aa.first,aa.second});
			}
			std::sort(authors_count_sorted.begin(), authors_count_sorted.end() , [](const std::pair<std::wstring,AuthorStreak>& a, const std::pair<std::wstring,AuthorStreak>& b)->bool { return a.second.count > b.second.count; });
			int printed=0;
			for(const auto& aa : authors_count_sorted) {
				std::string spaces{};
				int len=longest_author+1 - aa.first.size();
				while(--len>0) spaces+=" ";
				if(longest_author>3) {
					std::cout << spaces << converter.to_bytes(aa.first) << " : " << aa.second.count;
				} else {
					std::cout << std::setw(4) << aa.second.count;
				}
				std::cout << " Total commits\n";
				if(++printed >= opt.print_authors) break;
			}
		}
	}
	std::cout << "\n                                                                                         Less ";
	dot.put(0);dot.put(1);dot.put(2);dot.put(3);dot.put(4);
	std::cout << " More\n";
	std::cout << std::setw(4) << commits.size() << ": Total commits\n";
}

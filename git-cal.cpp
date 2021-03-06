/* this produces a git-calendar, like github or gitlab show for user activity.
 * See git-cal --help.
 *
 * Example scripts for .dotfiles:
 *
 * git-cal.bin --git-dir ~/.dotfiles/.git --work-tree=${HOME} "$@"
 * git-cal.bin --git-dir /home/.janek-git/.git --work-tree=/home/janek "$@"
 * git-cal.bin "$@"
 *
 */

#include "OptionsCal.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>
#include "boost/date_time/c_local_time_adjustor.hpp"
#include <algorithm>
#include <sstream>
#include <codecvt>
#include <boost/optional.hpp>
#include <sys/ioctl.h>

// FIXME - duplicate with small changes from file moj-git-status.cpp. Should fix later.
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
	// streaks are expressed in units of days_back
	std::set<int> streaks{};
};

typedef std::map<std::wstring,AuthorStreak> AuthorsCount;

struct DayInfo {
	int count;
	AuthorsCount authors;
};

std::pair<boost::optional<boost::posix_time::time_period> ,boost::optional<boost::posix_time::time_period> > calcStreak(const std::set<int>& streaks) {
	if(streaks.empty()) return {boost::none,boost::none};

// FIXME - duplicate from int main(), should fix this later
	auto        now_local_date = boost::posix_time::second_clock::local_time().date();
	// I need ptime, with truncated hours.
	boost::posix_time::ptime now_date_LOC{now_local_date};
// FIXME - end

	int cur_start{-1},cur_end{-1},lon_start{-1},lon_end{-1};//,tmp_start{-1},tmp_end{-1};

// This loop can be done better.
	std::vector<int> cal( *streaks.rbegin()+1 , 0);
	for(const auto& i : streaks) { cal[i] = 1; }
	for(size_t i=1 ; i<cal.size() ; ++i) { if((cal[i] != 0) and (cal[i-1] != 0)) cal[i] = cal[i-1]+1; }
	if(cal[0] != 0) {
		cur_end = 0;
		for(size_t i=1 ; i<cal.size() ; ++i) {
			if(cal[i] == 0) {
				cur_start = i-1;
				break;
			}
		}
	}
	int max = *std::max_element( cal.begin() , cal.end() );
	if(max >= 1) {
		for(size_t i=1 ; i<cal.size() ; ++i) {
			if(cal[i] == max) {
				lon_start = i;
				lon_end   = i-max+1;
			}
		}
	}

/* some other method but it has some error inside, for example yade Bruno commmits from year 2014.
	for(const auto& i : streaks_sorted) {
		if(tmp_start == -1) {
			tmp_end   = i;
			tmp_start = i;
		} else {
			if( std::abs(i - tmp_start) == 1) {
				tmp_start = i;
			} else {
				if(tmp_end   == 0) {
					cur_start = tmp_start;
					cur_end   = tmp_end;
				}
				if( std::abs(tmp_start - tmp_end) >= std::abs(lon_start - lon_end) ) {
					lon_start = tmp_start;
					lon_end   = tmp_end;
				}
				tmp_start = i;
				tmp_end   = i;
			}
		}
	}
	// oops start↔end got swapped, because numbers inside std::set<int> go back
	//std::swap(lon_end,lon_start);
	//std::swap(cur_end,cur_start);
*/

	boost::posix_time::ptime then1 = now_date_LOC - boost::posix_time::time_duration(boost::posix_time::hours(24*(lon_start  )));
	boost::posix_time::ptime then2 = now_date_LOC - boost::posix_time::time_duration(boost::posix_time::hours(24*(lon_end  -1)));
	boost::posix_time::ptime then3 = now_date_LOC - boost::posix_time::time_duration(boost::posix_time::hours(24*(cur_start  )));
	boost::posix_time::ptime then4 = now_date_LOC - boost::posix_time::time_duration(boost::posix_time::hours(24*(cur_end  -1)));
	boost::optional<boost::posix_time::time_period> ret1=boost::none; ret1=boost::optional<boost::posix_time::time_period>({then1 , then2});
	boost::optional<boost::posix_time::time_period> ret2=boost::none;
	if(cur_end   != -1) {
		ret2 = boost::optional<boost::posix_time::time_period>({ then3 , then4 });
	}
	return { ret1 , ret2 };
}

void printStreaks(const std::set<int>& streaks) {
	std::pair<boost::optional<boost::posix_time::time_period> ,boost::optional<boost::posix_time::time_period> > stt = calcStreak(streaks);
	if(stt.first) {
		std::cout << " Longest streak "<< std::ceil(double(stt.first.get().length().hours())/24.0)
		<< " days (" << stt.first.get().begin().date() << " " << stt.first.get().last().date() << ").";
		if(stt.second) {
			std::cout << " Current streak: " << std::ceil(double(stt.second.get().length().hours())/24.0) <<" days";
		}
	}
	std::cout << "\n";
}

void printAuthorSummary(const AuthorsCount& authors_count, size_t longest_author, const OptionsCal& opt) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter; // FIXME - duplicate from int main(), because I am lazy
	std::vector<std::pair<std::wstring,AuthorStreak> > authors_count_sorted{};
	for(const auto& aa : authors_count) {
		authors_count_sorted.push_back({aa.first,aa.second});
	}
	std::sort(authors_count_sorted.begin(), authors_count_sorted.end() ,
		[](const std::pair<std::wstring,AuthorStreak>& a, const std::pair<std::wstring,AuthorStreak>& b)->bool { return a.second.count > b.second.count; });
	int printed=0;
	for(const auto& aa : authors_count_sorted) {
		std::string spaces{};
		int len=longest_author+1 - aa.first.size();
		while(--len>0) spaces+=" ";
		if(longest_author>3) {
			std::cout << spaces << converter.to_bytes(aa.first) << " : " << std::setw(4) << aa.second.count << " ";
		} else {
			std::cout << std::setw(4) << aa.second.count << " ";
		}
		std::cout << "total commits.";
		printStreaks(aa.second.streaks);
		if(++printed >= opt.print_authors) break;
	}
}

struct Dot {
	int q1{0},q2{0},q3{0};
	const std::string esc{char{27}}; // I would just use std::cout << "\e", but it is generating compiler warnings, so lets define the string with ESC inside.
	const std::vector<int> colors={ 237, 139, 40, 190, 1 };
	Dot(const std::vector<DayInfo>& cc) {
		std::vector<int> vv{};
		for(const auto& a:cc) { if(a.count != 0) vv.push_back(a.count); }
		std::sort(vv.begin(),vv.end());
		size_t s = vv.size();
//	std::cout << s << " ← size\n";
		if(s!=0) {
			q1=vv[s/4];
			q2=vv[s/2];
			q3=vv[3*s/4];
		} else {
			std::cerr << "Error: there are zero commits.\n";
			exit(1);
		}
//		std::cerr << vv[0] << " quartiles: " << q1 << " " << q2 << " " << q3 << "\n";
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

void calcCurrentWeek(int& current_week,const boost::posix_time::ptime& now_date_LOC, const OptionsCal& opt) {
	boost::posix_time::ptime year_start(boost::gregorian::date(now_date_LOC.date().year() , 1 , 1 ));
	current_week = 0;// weeks start with 0
	while(year_start < now_date_LOC) {
		year_start += boost::posix_time::time_duration(boost::posix_time::hours(24));
		if(     opt.start_with_sunday  and year_start.date().day_of_week() == 0) { ++current_week; };
		if((not opt.start_with_sunday) and year_start.date().day_of_week() == 1) { ++current_week; };
	}
}

void decrement(int& years_back, int& current_week, int& day_of_week, const OptionsCal& opt, int WEEKS, const boost::posix_time::ptime& now_date_LOC, int days_back) {
	if(opt.use_calendar_years) {
		boost::posix_time::ptime then = now_date_LOC - boost::posix_time::time_duration(boost::posix_time::hours(24 * days_back));
		//calcCurrentWeek(current_week , then , opt);
		day_of_week = then.date().day_of_week();
		if(not opt.start_with_sunday) { day_of_week = (day_of_week+6)%7;}
		int new_years_back  = now_date_LOC.date().year() - then.date().year();
		if(new_years_back != years_back) {
			calcCurrentWeek(current_week , then , opt);
			years_back = new_years_back;
		} else {
			if(day_of_week == 6) { --current_week; }
		}
	} else {
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
}

int main(int argc, char** argv)
{
// FIXME - this int main() should be split into several functions inside a class, so that some variables are not duplicated: now_local_date, now_date_LOC, converter

// Parse command line options
	struct winsize ww; ioctl(0, TIOCGWINSZ, &ww);
	OptionsCal opt(argc,argv,ww.ws_col,15);
	if(opt.number_commits and opt.number_days) {
		std::cerr << "Bad invocation: `--number-days,-n`  and  `--number-commits,-c`  are mutually exclusive.\n";
		exit(1);
	}
	if(opt.include_emails and (opt.print_authors == 0)) {
		std::cerr << "Bad invocation: `--include-emails,-e` cannot work when `--print-authors,-N` == 0.\n";
		exit(1);
	}
// This line will print the options after parsing.
//	opt.print();
// Prepare for git invocation
	std::string gdir  = ""; if(opt.git_dir   != "") gdir  = " --git-dir "  +opt.git_dir;
	std::string wtree = ""; if(opt.work_tree != "") wtree = " --work-tree "+opt.work_tree;
	std::string author= ""; if(opt.author    != "") author= " --author='"   +opt.author+"'";
	std::string call  = "/usr/bin/git "+gdir+wtree;

// Calculate how many years passed since year 1975, also record present time `now` in UTC as well as in local clock. Will be used later too.
	boost::posix_time::ptime date1970(boost::gregorian::date(1970,1,1));
	auto        now            = boost::posix_time::second_clock::universal_time();
	auto        now_local      = boost::posix_time::second_clock::local_time();
	auto        now_local_date = now_local.date();
	boost::posix_time::ptime now_date_UTC{now.date()};
	boost::posix_time::ptime now_date_LOC{now_local_date};
	int         years_max      = (now - date1970).hours()/24/365.25 - 5; // FIXME : better use now.date().year() - 1975; why did I write it this way earlier??
	// if only_last_year then reset years_max to 1.
	if(opt.only_last_year) { years_max=1; }
// https://git-scm.com/docs/pretty-formats
// XXX: Thinking about adding the ability to count the number of lines in each commit, then for example filtering git-cal to show only commits within some range of committed lines.
//      so far I found the --numstat option
//      git log --no-merges --pretty=format:"%at %aE, %aN" --since="13 months" --numstat
//      also I found this to count number of lines, but it has mistakes:
//        git ls-files -z | xargs -0n1 git blame -w | ruby -n -e '$_ =~ /^.*\((.*?)\s[\d]{4}/; puts $1.strip' | sort -f | uniq -c | sort -n
// Prepare --pretty=format for git invocation
	std::string fmt="";
	if(opt.print_authors != 0) {
		fmt = " %aN";
		if(opt.include_emails) {
			fmt = " %aE, %aN";
		}
	}
// invoke git
	std::string git_cal_result = exec(call+" log --no-merges --pretty=format:\"%at"+fmt+"\" --since=\""+(boost::lexical_cast<std::string>(years_max))+" years\""+author);

// Parse git output and store it into commits
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
// Make sure it is sorted. Git produces reverse sorted output, so this isn't maybe strictly necessary, it's just to make sure.
	std::sort(commits.rbegin() , commits.rend() , [](const CommitInfo& a, const CommitInfo& b)->bool { return a.time < b.time; } );
	auto start = commits.rbegin()->time;
// Round up the number of days to display on screen, so that it starts at a full year
	int WEEKS = 52 + 2*int(opt.use_calendar_years); // when use_calendar_years max is 54 weeks
	int days  = (now-start).hours()/24+1;
	int years = std::ceil(double(days)/(WEEKS*7));//(365.25));
	if(opt.use_calendar_years) {
		years = now_local_date.year() - start.date().year() + 1;
//		std::cerr << "years: " << years << "\n";
	}
	int day_of_week  = now_local_date.day_of_week();
	std::string weeknames=" M W F ";
	if(not opt.start_with_sunday) {
		day_of_week = (day_of_week+6)%7;
		weeknames="M W F S";
	}
	// the final calculation. Without this line the calendar printed on screen whould abruptly end upon the first commit, instead at the year's start.
	days=years*WEEKS*7 - (6-day_of_week);
	if(opt.use_calendar_years) {
		days = (now_date_LOC - boost::posix_time::ptime(boost::gregorian::date(start.date().year(),1,1))).hours()/24 + 1;
	}
// Calculate how many commits were done per day, also track each author separetely
	std::vector<DayInfo> count_per_day(days , {0,{}} );
	for(auto& that_commit_UTC : commits) {
//std::cerr << that_commit_UTC.time << " " << ( that_commit_UTC.time - diff_UTC_local ) << "\n";
//		int idx = ( now_date_UTC - boost::posix_time::ptime( that_commit_UTC.time                   .date()) ).hours()/24;
		// I need difference between dates with truncated hours, so that one second after midnight becomes full 24 hours.
		int idx = ( now_date_LOC - boost::posix_time::ptime((boost::date_time::c_local_adjustor<boost::posix_time::ptime>::utc_to_local(that_commit_UTC.time)).date()) ).hours()/24;
		if((idx >=0) and (idx < int(count_per_day.size()))) {
			count_per_day[ idx ].count += 1;
			count_per_day[ idx ].authors[that_commit_UTC.author].count += 1;
		}
	}

// The Dot class will print colored ◼ or commit counts or day of the month
	Dot dot(count_per_day);
/*
	std::cout << "today is: " << now_local_date.day_of_week()  << "\n";
	std::cout << "today is: " << now_local_date.month()  << "\n";
	std::cout << "today is: " << now_local_date.year()  << "\n";
*/
// Now fill the calendar matrix. Assume each year made of 52 weeks. The stored value is days_back since today. Also it refers to position in count_per_day[days_back]
	// ↓ year      ↓ week       ↓ day   days_back
	std::vector<std::vector<std::vector<int>>> calendar(years,std::vector<std::vector<int>>(WEEKS,std::vector<int>(7,-1))); // 3D matrix is constructed at the exactly needed size, filled with -1
	int current_week = WEEKS-1;
	if(opt.use_calendar_years) { calcCurrentWeek(current_week,now_date_LOC,opt); }
	int years_back   = 0;
	for(size_t i = 0 ; i<count_per_day.size() ; ++i) {
//std::cerr << "week: " << current_week << " years_back: " << years_back << "\n";
		assert(years_back   >=0 and years_back   < years);
		assert(current_week >=0 and current_week < WEEKS);
		assert(day_of_week  >=0 and day_of_week  < 7    );
		calendar[years_back][current_week][day_of_week] = i;
		decrement(years_back, current_week, day_of_week, opt, WEEKS, now_date_LOC , i+1);
	}

// Now print the calendar matrix on screen, calculate author contributions along the way for each year and sort them in authors_count
	ss.str("");
	std::set<int> total_streaks{};
	AuthorsCount total_authors_count{};
	for(int y = years-1 ; y>=0 ; --y) {
		bool just_printed=false;
		bool got_year=false;
		int prev_month=-1;
		AuthorsCount authors_count;
		std::string months_str("      ");// will write the Month names after these spaces
		std::string year_str(months_str);
		// This loop is only to construct the YEAR and MONTH lines
		for(int w=0 ; w<WEEKS ; ++w) {
			int days_back = calendar[y][w][0];
			if(days_back != -1) {
				boost::posix_time::ptime then = now_date_LOC - boost::posix_time::time_duration(boost::posix_time::hours(24*days_back));
				int new_month = then.date().month();
				if( (not got_year) and year_str.size() >= 56 ) {
					year_str.resize(56);
					// puts a year number above the month which is in the middle
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
		if(not got_year) {
			year_str.resize(56,' ');
			year_str += boost::lexical_cast<std::string>(now_date_LOC.date().year());
		}
		std::cout << year_str << "\n" << months_str << "\n";
		// This loop prints the actual colored ◼, and calculates commits
		for(int d=0 ; d<7 ; ++d) {
			std::cout << "    "<<weeknames[d]<<" ";
			for(int w=0 ; w<WEEKS ; ++w) {
				int days_back = calendar[y][w][d];
				if(days_back != -1) {
					assert(days_back >=0 and days_back<days);
					int val = count_per_day[days_back].count;
					for(const auto& aa : count_per_day[days_back].authors) {
						authors_count[aa.first].count += aa.second.count;
						authors_count[aa.first].streaks.insert(days_back);
						total_authors_count[aa.first].count += aa.second.count;
						total_authors_count[aa.first].streaks.insert(days_back);
						total_streaks.insert(days_back);
					}
					boost::posix_time::ptime then = now_date_LOC - boost::posix_time::time_duration(boost::posix_time::hours(24*days_back));
					dot.print(val,then,opt);
				} else {
					std::cout << "  ";
				}
			}
			std::cout << "\n";
		}
		// When whole year is printed, then print the summary for this year
		if(opt.print_authors != 0 or opt.print_streaks) {
			printAuthorSummary(authors_count , longest_author, opt);
		}
	}
	std::cout << "\n                                                                                         Less ";
	dot.put(0);dot.put(1);dot.put(2);dot.put(3);dot.put(4);
	std::cout << " More\n";
	std::cout << std::setw(4) << commits.size() << " total commits since "<< start.date() <<".";
	printStreaks(total_streaks);
	std::cout << "\n";

	if(opt.print_authors != 0) {
		printAuthorSummary(total_authors_count , longest_author, opt);
	}
}

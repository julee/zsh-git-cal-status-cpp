/* program:
 *
 * 1. zakłada unikalny lockfile(…) dla podanych argumentów + whoami + pwd
 *    → jesli uda się go założyć to wywołuje git status --porcelain --branch --git-dir ARG1 --work-tree ARG2
 *    Zwraca:
 *      [ branch , ahead , behind , staged    , conflicts , changed , untracked , kod_błędu ]
 *    Tylko gdy kod_błędu != 0, to reszta ma sens, w przeciwnym razie wskazuje co poszło źle w wywołaniu.
 *
 * 2. jeśli nie udało się założyć lockfile to:
 *    Zwraca wynik poprzedniego wywołania, jeśli taki istnieje
 *      [ branch , ahead , behind , staged    , conflicts , changed , untracked , kod_błędu ]
 *    W przeciwnym razie zwraca tylko kod błędu:
 *      [ branch ,   0   ,    0   ,     0     ,   0       ,    0    ,     0     , kod_błędu ]
 *
 * kody_błędów:
 *    0      : nie ma problemów
 *    101001 : brakuje --pwd_dir
 *    101002 : nie moze otworzyc pliku bufora
 *    -N     : wynik odczytany z bufora N sekund temu (pewnie się teraz akurat liczy nowy git status i jeszcze nie skończył).
 *    101002 : różnica w sekundach miała wyjść <=0 a nie wyszła.
 *
 * przykłądowe wywołania:

moj-git-status --git-dir /home/.janek-git/.git --work-tree=/home/janek --pwd-dir `pwd`
moj-git-status --git-dir /home/.janek-git/.git --work-tree=/home/janek --pwd-dir `pwd` --branch-master-override jg

moj-git-status --git-dir ~/.dotfiles/.git --work-tree=$HOME --pwd-dir `pwd`
moj-git-status --git-dir ~/.dotfiles/.git --work-tree=$HOME --pwd-dir `pwd` --branch-master-override dg

moj-git-status --pwd-dir `pwd`

 *
 *
 */

#include "Options.hpp"

#include <stdexcept>
#include <sys/ioctl.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/lexical_cast.hpp>
#include <sys/stat.h>
#include <boost/date_time/posix_time/conversion.hpp> // boost:::to_time_t(ptime pt) - ilość sekund od 1970.
#include <boost/date_time/posix_time/posix_time.hpp>

//#include <boost/filesystem/operations.hpp> // rezygnuję, bo trzeba linkować.

std::map<int,std::string> error_codes = {{0,"ok"},{101001,"brakuje --pwd_dir"},{101002,"nie moze otworzyc pliku bufora"},{101002,"różnica w sekundach miała wyjść <=0 a nie wyszła"}};

std::string sanitize(std::string offending_string) {
	std::string extr=":+-.=_,"; // () <> & na pewno nie mogą być, co do innych to nie wiem
	std::replace_if(offending_string.begin(), offending_string.end(), [&extr](char ch)->bool{ return not(std::isalpha(ch) or std::isdigit(ch) or extr.find(ch)!=std::string::npos); } , ':');
	return offending_string;
}

struct ExecError : std::runtime_error {
	ExecError(int x) : std::runtime_error("exec failed") , code{x} { }
	int code;
	const char* what() const noexcept override { return "exec error"; }
};

// https://stackoverflow.com/questions/52164723/how-to-execute-a-command-and-get-return-code-stdout-and-stderr-of-command-in-c
// FIXME: lepsze to, ale nie działa tego nie ma w boost 1.62, jest w 1.68, a ja mam tutaj 1.62
//        https://www.boost.org/doc/libs/1_65_1/doc/html/boost_process/tutorial.html#boost_process.tutorial.starting_a_process
std::string exec(const std::string& cmd, int code) {
/* ajajaj, tego nie ma w boost 1.62, jest w 1.68, a ja mam tutaj 1.62
#include <iostream>
#include <boost/process/system.hpp>
int main(int argc, char** argv)
{
	boost::process::ipstream std_out;
	boost::process::ipstream std_err;
	boost::process::system("ls -laR /tmp", bp::std_out > std_out, bp::std_err > std_err, bp::std_in < stdin);
}
*/

    std::array<char, 128> buffer;
    std::string result;
    auto pipe = popen(cmd.c_str(), "r"); // get rid of shared_ptr
    if (!pipe) throw ExecError(code + 0);
    while (!feof(pipe)) {
        if (fgets(buffer.data(), 128, pipe) != nullptr)
            result += buffer.data();
    }
    auto rc = pclose(pipe);
    if (rc == EXIT_SUCCESS) { // == 0
	return result;
    } else {
	//return result;
	throw ExecError(code+rc);
    }
}

const std::vector<std::string> conflict_strings{"DD","AU","UD","UA","DU","AA","UU"};
class GitParse {
	private:
		std::string	branch    = "testuje";
		int		ahead     = 0;
		int		behind    = 0;
		int		staged    = 0;
		int		conflicts = 0;
		int		changed   = 0;
		int		untracked = 0;

	public:
		void parse(std::string s) {
			std::string start = s.substr(0,2);
			if(start == "##") {
				if (s == "## Initial commit on master") {
					branch="master";
					return;
				}
				std::size_t pos_dots = s.find("...");
				if (pos_dots != std::string::npos) {
					branch=sanitize(s.substr(3,pos_dots-3));
					std::size_t pos_ahead  = s.find("ahead");
					if (pos_ahead != std::string::npos) {
						std::istringstream ss(s.substr(pos_ahead  + 6));
						ss >> ahead;
					}
					std::size_t pos_behind = s.find("behind");
					if (pos_behind != std::string::npos) {
						std::istringstream ss(s.substr(pos_behind + 7));
						ss >> behind;
					}
				} else {
					branch=sanitize(s.substr(3));
				}
			} else {
				if(start == "??") {
					++ untracked;
					return;
				}
				if( find( conflict_strings.begin() , conflict_strings.end() , start ) != conflict_strings.end() ) {
					++ conflicts;
					return;
				}
				if(start[0] != ' ') {
					++ staged;
				}
				if(start[1] != ' ') {
					++ changed;
				}
			}
		}
		std::string str(const Options& opt) {
			if((not opt.branch_master_override.empty()) and branch=="master") branch = opt.branch_master_override;
			return       branch
				+" "+boost::lexical_cast<std::string>(ahead     )
				+" "+boost::lexical_cast<std::string>(behind    )
				+" "+boost::lexical_cast<std::string>(staged    )
				+" "+boost::lexical_cast<std::string>(conflicts )
				+" "+boost::lexical_cast<std::string>(changed   )
				+" "+boost::lexical_cast<std::string>(untracked );
		}
};

std::string gitParsedResult(const Options& opt) {
	std::string gdir  =""; if(opt.git_dir   != "") gdir  = " --git-dir "  +opt.git_dir;
	std::string wtree =""; if(opt.work_tree != "") wtree = " --work-tree "+opt.work_tree;
	std::string git_porcelain = exec("/usr/bin/git "+gdir+wtree+" status --porcelain --branch",0);

	GitParse result;
	std::string line;
	std::istringstream ss_result(git_porcelain);
	while (getline(ss_result, line, '\n')) {
		result.parse(line);
	}

	return result.str(opt);
}

int main(int argc, char** argv)
try {
	struct winsize w; ioctl(0, TIOCGWINSZ, &w);
	Options opt(argc,argv,w.ws_col,15);
//std::cerr << "pwd dir  : " << opt.pwd_dir   << "\n";
//std::cerr << "git dir  : " << opt.git_dir   << "\n";
//std::cerr << "work tree: " << opt.work_tree << "\n";
	if(opt.pwd_dir == "") throw ExecError(101001);
	std::string whoami = exec("/usr/bin/whoami",100000);
//std::cerr << "whoami   : \"" << whoami     << "\"\n";
	std::string lockfile_name = sanitize(std::string("moj_git_status_PWD:"+opt.pwd_dir+"_WHO:"+whoami+"_DIR:"+opt.git_dir+"_TREE:"+opt.work_tree));
//std::cerr << lockfile_name << "\n";
	std::string lock_1st_fname = "/tmp/"+lockfile_name;
	std::string lock_2nd_fname = "/tmp/"+lockfile_name+"_RESULT";
	std::string touch  = exec("/usr/bin/touch "+lock_1st_fname,200000);
//std::cerr << "touch    : \"" << touch      << "\"\n";

	// https://www.boost.org/doc/libs/1_68_0/doc/html/boost/interprocess/file_lock.html
	boost::interprocess::file_lock the_1st_lock(lock_1st_fname.c_str());
	if(the_1st_lock.try_lock()) {
		std::string git_parsed_result = gitParsedResult(opt);
		std::ofstream result_file(lock_2nd_fname);
		boost::interprocess::file_lock the_2nd_lock(lock_2nd_fname.c_str());
		if(the_2nd_lock.try_lock()) {
			// mamy otwarty plik result_file, można do niego pisać.
			result_file << git_parsed_result;
		} else {
			// nie udało się do niego pisać, chociaż mamy wynik :(
		}
		std::cout << git_parsed_result << " 0"; // ostatnie zero oznacza brak błędów
	} else {
		// nie udało się zakluczyć, zwracamy zawartość result_file
		std::ifstream result_file(lock_2nd_fname);
		if(result_file.is_open()) {
			//std::time_t when = boost::filesystem::last_write_time(lock_2nd_fname); // rezygnuję, bo trzeba linkować.
			int older=-100;
			struct stat result;
			if(stat(lock_2nd_fname.c_str(), &result)==0) {
				auto mod_time = result.st_mtime;
				boost::posix_time::ptime when(boost::posix_time::from_time_t(mod_time)         );
				boost::posix_time::ptime now (boost::posix_time::second_clock::universal_time());
				older = (when-now).total_seconds(); // powinno wyjść ujemne.
				if(older > 0) throw ExecError(101003);
			};
			std::string line;
			getline(result_file , line);
			std::cout << line << " " << older; // FIXME - czas w sekundach tu ma być.
		} else {
			throw ExecError(101002);
		}
	}

} catch(const ExecError& err) {
//std::cerr << "Error code " << err.code;
	auto it = error_codes.find(err.code);
	if(it != error_codes.end()) {
//std::cerr << " : " << it->second << "\n";
	} else {
//std::cerr << " : unrecognized code\n";
	}
	std::cout << "unknown 0 0 0 0 0 0 " << err.code;
}



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
 *    101003 : różnica w sekundach miała wyjść <=0 a nie wyszła.
 *    101004 : cannot stat file.
 *    101005 : brakuje whoami
 *    101006 : failed to spawn orphan
 *    -N     : wynik odczytany z bufora N sekund temu (pewnie się teraz akurat liczy nowy git status i jeszcze nie skończył).
 *    -100   : wynik specjalny - gdy nie udało się sprawdzić daty na pliku bufora.
 *
 * przykłądowe wywołania:

moj-git-status.bin --whoami $(whoami) --pwd-dir JGIT --git-dir /home/.janek-git/.git --work-tree=/home/janek
moj-git-status.bin --whoami $(whoami) --pwd-dir JGIT --git-dir /home/.janek-git/.git --work-tree=/home/janek --branch-master-override jg

moj-git-status.bin --whoami $(whoami) --pwd-dir DGIT --git-dir ~/.dotfiles/.git --work-tree=$HOME
moj-git-status.bin --whoami $(whoami) --pwd-dir DGIT --git-dir ~/.dotfiles/.git --work-tree=$HOME --branch-master-override dg

moj-git-status.bin --whoami $(whoami) --pwd-dir ${PWD:A}

 *
 *
 */

#include "Options.hpp"

#include <stdexcept>
#include <sys/wait.h>
//#include <sys/ioctl.h>
#include <iostream>
#include <fstream>
//#include <sstream>
//#include <map>
//#include <vector>
#include <boost/interprocess/sync/file_lock.hpp>
//#include <boost/lexical_cast.hpp>
//#include <sys/stat.h>
//#include <boost/date_time/posix_time/conversion.hpp> // boost:::to_time_t(ptime pt) - ilość sekund od 1970.
#include <boost/date_time/posix_time/posix_time.hpp>

//#include <boost/filesystem/operations.hpp> // rezygnuję, bo trzeba linkować.

std::map<int,std::string> error_codes = {
	  {0      , "ok" }
	, {101001 , "brakuje --pwd_dir" }
	, {101002 , "nie moze otworzyc pliku bufora" }
	, {101002 , "różnica w sekundach miała wyjść <=0 a nie wyszła" }
	, {101004 , "cannot stat file" }
	, {101005 , "brakuje whoami" }
	, {101006 , "failed to spawn orphan" }
	};

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
// FIXME: lepsze to, ale nie działa tego nie ma w boost 1.62, jest dopiero od 1.64, a ja mam tutaj 1.62
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
		void parse(const std::string& s) {
			std::string start = s.substr(0,2);
			if(start == "##") {
				if (s == "## Initial commit on master") {
					branch="master";
					return;
				}
				std::size_t pos_dots = s.find("...");
				if (pos_dots != std::string::npos) {
					branch=/*sanitize(*/s.substr(3,pos_dots-3)/*)*/;
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
					branch=/*sanitize(*/s.substr(3)/*)*/;
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

int fileOlderSeconds(const std::string& fn /*,bool do_throw*/) {
	struct stat result;
	if(stat(fn.c_str(), &result)==0) {
		auto mod_time = result.st_mtime;
		boost::posix_time::ptime when(boost::posix_time::from_time_t(mod_time)         );
		boost::posix_time::ptime now (boost::posix_time::second_clock::universal_time());
		int older = (when-now).total_seconds(); // powinno wyjść ujemne.
		if(older > 0) {
		/*
			if(do_throw) {
				throw ExecError(101003);
			} else
		*/
			{
				return 1;
			}
		}
		return older;
	};
/*
	if(do_throw) {
		throw ExecError(101004);
	} else
*/
	{
		return 1;
	}
}

#if 0
// https://stackoverflow.com/questions/17599096/how-to-spawn-child-processes-that-dont-die-with-parent
// Niestety nie działa wewnątrz zsh. Tzn odpalany niezależnie - działa. Ale wewnątrz zsh się nie potrafi zforkować / zespawnować.
bool spawn_orphan(const std::string& cmd) {
	//char command[1024]; // We could segfault if cmd is longer than 1000 bytes or so
	if(cmd.size() > 1000) {
		return false;
	}
	int pid;
	int Stat;
	pid = fork();
	if (pid < 0) { // perror("FORK FAILED\n"); return pid;
		return false;
	}
	if (pid == 0) { // CHILD
		setsid(); // Make this process the session leader of a new session
		pid = fork();
		if (pid < 0) { //printf("FORK FAILED\n"); exit(1);
			exit(1);
		}
		if (pid == 0) { // GRANDCHILD
			//sprintf(command,"bash -c '%s'",cmd);
			execl("/bin/sh", "/bin/sh", "-c", cmd.c_str(), NULL); // Only returns on error
			//perror("execl failed");
			// system(cmd.c_str()); // tego mocno odradzają.
			exit(1);
		}
		exit(0); // SUCCESS (This child is reaped below with waitpid())
	}

	// Reap the child, leaving the grandchild to be inherited by init
	waitpid(pid, &Stat, 0);
	if ( WIFEXITED(Stat) && (WEXITSTATUS(Stat) == 0) ) {
		return true; // Child forked and exited successfully
	} else {
		//perror("failed to spawn orphan\n");
		//return -1;
		return false;
	}
}

/* bo tamten nie chce działać wewnątrz zsh
bool inny_spawn_orphan(const std::string& cmd) {
}
*/

void spawn_myself(const Options& opt, char** argv) {
	std::string command = argv[0];
	command = "/home/"+opt.whoami+"/bin/"+command+opt.rebuild()+" --must-update-now";
	//std::cerr << "\n-calling-\n" << command << "\n--\n";
	// Niestety nie działa wewnątrz zsh. Tzn odpalany niezależnie - działa. Ale wewnątrz zsh się nie potrafi zforkować / zespawnować.
	if(not spawn_orphan(command)) {
		throw ExecError("cannot_spawn");//(101006);
	}
}
#endif

void update_git_status(const std::string& lock_1st_fname, const std::string& lock_2nd_fname, const std::string& lock_3rd_fname, const Options& opt) {
	//std::cerr << "\n-must_update_now-\n";
	std::string touch  = exec("/usr/bin/touch "+lock_1st_fname,200000);  //std::cerr << "touch    : \"" << touch      << "\"\n";
	// https://www.boost.org/doc/libs/1_68_0/doc/html/boost/interprocess/file_lock.html
	boost::interprocess::file_lock the_1st_lock(lock_1st_fname.c_str());
	if(the_1st_lock.try_lock()) {
		std::ofstream result_file(lock_2nd_fname);
		boost::interprocess::file_lock the_2nd_lock(lock_2nd_fname.c_str());
		if(the_2nd_lock.try_lock()) {
			// mamy otwarty plik result_file, można do niego pisać.
			std::string git_parsed_result = gitParsedResult(opt);
			result_file << git_parsed_result;
			result_file.close();
			the_2nd_lock.unlock();
			std::string command = "/bin/mv -f "+lock_2nd_fname+" "+lock_3rd_fname;
			system(command.c_str());
		} // else // nie udało się do niego pisać, chociaż mamy wynik :(
	//	std::cout << git_parsed_result << " 0"; // ostatnie zero oznacza brak błędów
	}
}

int main(int argc, char** argv)
try {
//	struct winsize w; ioctl(0, TIOCGWINSZ, &w);
	Options opt(argc,argv/*,w.ws_col,15*/);//std::cerr << "pwd dir  : " << opt.pwd_dir << "\n"; //std::cerr << "git dir  : " << opt.git_dir << "\n";//std::cerr << "work tree: " << opt.work_tree << "\n";
	if(opt.pwd_dir == "") throw ExecError(101001);
	if(opt.whoami  == "") throw ExecError(101005);
	//std::string whoami = exec("/usr/bin/whoami",100000);  //std::cerr << "whoami   : \"" << whoami     << "\"\n";
	std::string lockfile_name = sanitize(std::string("moj_git_status_PWD:"+opt.pwd_dir+"_WHO:"+opt.whoami+"_DIR:"+opt.git_dir+"_TREE:"+opt.work_tree));  //std::cerr << lockfile_name << "\n";
	std::string lock_1st_fname = "/tmp/"+lockfile_name;
	std::string lock_2nd_fname = "/tmp/"+lockfile_name+"_RESULT";
	std::string lock_3rd_fname = "/tmp/"+lockfile_name+"_COPY";

	if(not opt.must_update_now) {
		int older = fileOlderSeconds(lock_3rd_fname);
		if(-older > opt.refresh_sec or older == 1) { // forced refresh
#if 0
			// Niestety nie działa wewnątrz zsh. Tzn odpalany niezależnie - działa. Ale wewnątrz zsh się nie potrafi zforkować / zespawnować.
			spawn_myself(opt,argv);
#endif
			update_git_status(lock_1st_fname,lock_2nd_fname,lock_3rd_fname,opt);
		}
		{
			// nie udało się zakluczyć, lub nie musimy odświeżać, zwracamy zawartość result_file
			std::ifstream result_file(lock_3rd_fname);
			if(result_file.is_open()) {
				std::string line;
				getline(result_file , line);
				std::cout << line << " " << older;
				return 0;
			} else {
				throw ExecError(101002);
			}
		}
	} else {
		update_git_status(lock_1st_fname,lock_2nd_fname,lock_3rd_fname,opt);
	}


} catch(const ExecError& err) {
//std::cerr << "Error code " << err.code;
	if(err.code == 32768) { // fatal: Not a git repository - nie ma nic do pisnia.
		return 0;
	}
	auto it = error_codes.find(err.code);
	if(it != error_codes.end()) {
//std::cerr << " : " << it->second << "\n";
	} else {
//std::cerr << " : unrecognized code\n";
	}
	std::cout << "err"+boost::lexical_cast<std::string>(err.code)+" 0 0 0 0 0 0 " << err.code;
	return err.code;
}


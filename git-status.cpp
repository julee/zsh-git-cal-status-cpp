/* how it works:
 *
 * 1. creates unique lockfile(…) for given arguments + whoami + pwd
 *    → if that was successful then calls in the shell: git status --porcelain --branch --git-dir ARG1 --work-tree ARG2
 *    returns:
 *      [ branch_name , ahead , behind , staged    , conflicts , changed , untracked , error_code ]
 *    Only when the error_code == 0, the rest has any meaning, otherwise it indicates what wne wrong.
 *    It stores the result in a file in /tmp/ see variable result_file below
 *
 * 2. if lockfile creation was unsuccesfull then:
 *    Returns the result of previous call if the file in /tmp/ exists:
 *      [ branch_name , ahead , behind , staged    , conflicts , changed , untracked , error_code ]
 *    Othwersie it's just an error:
 *      [ branch_name ,   0   ,    0   ,     0     ,   0       ,    0    ,     0     , error_code ]
 *
 * error_code (negative values are not fatal error, just extra information):
 *    0      : OK
 *    101001 : --pwd_dir is missing
 *    101002 : cannot open the cached file in /tmp
 *    101003 : difference in seconds should be <=0, but it is not: the cached file in /tmp/ is newer than current system time.
 *    101004 : cannot stat file.
 *    101005 : --whoami is missing
 *    101006 : failed to spawn orphan
 *    -N     : the result was read from cached file in /tmp which is N seconds old (probably git ststus is being run right now in other instance, the files are locked, and it didn't finish yet).
 *    -100   : special error: it was impossible to read the date of the cached file in /tmp.
 *
 * example usage:

# to use on home or documents directory:
git-status.bin --whoami $(whoami) --pwd-dir JGIT --git-dir /home/.janek-git/.git --work-tree=/home/janek
git-status.bin --whoami $(whoami) --pwd-dir JGIT --git-dir /home/.janek-git/.git --work-tree=/home/janek --branch-master-override jg

# to use on .dotfiles directory
git-status.bin --whoami $(whoami) --pwd-dir DGIT --git-dir ~/.dotfiles/.git --work-tree=$HOME
git-status.bin --whoami $(whoami) --pwd-dir DGIT --git-dir ~/.dotfiles/.git --work-tree=$HOME --branch-master-override dg

# to use on project in present directory
git-status.bin --whoami $(whoami) --pwd-dir ${PWD:A}

 */

#include "Options.hpp"

#include <stdexcept>
#include <sys/wait.h>
#include <iostream>
#include <fstream>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

// this short function removes characters forbidden by zsh
std::string sanitize(std::string offending_string) {
	std::string extr=":+-.=_,"; // the () <> & cannot be present for sure, about others I don't know.
	std::replace_if(offending_string.begin(), offending_string.end(), [&extr](char ch)->bool{ return not(std::isalpha(ch) or std::isdigit(ch) or extr.find(ch)!=std::string::npos); } , ':');
	return offending_string;
}

// small class to throw errors nicely.
struct ExecError : std::runtime_error {
	ExecError(int x) : std::runtime_error("exec failed") , code{x} , msg{} { }
	ExecError(const char* s) : std::runtime_error("exec failed") , code{0} , msg{s} { }
	int code;
	std::string msg;
	const char* what() const noexcept override { return "exec error"; }
};

// source: https://stackoverflow.com/questions/52164723/how-to-execute-a-command-and-get-return-code-stdout-and-stderr-of-command-in-c
std::string exec(const std::string& cmd, int code) {
/* this unfortunately requires boost 1.68, but I have 1.62 here. And it would be MUCH shorter, just one line with boost::process::system(...)
//        https://www.boost.org/doc/libs/1_65_1/doc/html/boost_process/tutorial.html#boost_process.tutorial.starting_a_process
#include <iostream>
#include <boost/process/system.hpp>
int main(int argc, char** argv)
{
	boost::process::ipstream std_out;
	boost::process::ipstream std_err;
	boost::process::system("ls -laR /tmp", bp::std_out > std_out, bp::std_err > std_err, bp::std_in < stdin);
}
*/
// so I will use a generic method
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
	throw ExecError(code + rc);
    }
}

const std::vector<std::string> conflict_strings{"DD","AU","UD","UA","DU","AA","UU"};
class GitParse {
	private:
		std::string	branch    = "testing";
		int		ahead     = 0;
		int		behind    = 0;
		int		staged    = 0;
		int		conflicts = 0;
		int		changed   = 0;
		int		untracked = 0;

	public:
		void parse(const std::string& s , const std::string& call) {
			std::string start = s.substr(0,2);
			if(start == "##") {
				if (s == "## Initial commit on master") {
					branch = "master";
					return;
				}
				if (s == "## HEAD (no branch)") {
					branch = /*sanitize(*/exec(call+" rev-parse --short HEAD",0)/*)*/;
					branch = branch.substr(0,branch.size()-1);
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
					branch = sanitize(s.substr(3));
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
	std::string gdir  = ""; if(opt.git_dir   != "") gdir  = " --git-dir "  +opt.git_dir;
	std::string wtree = ""; if(opt.work_tree != "") wtree = " --work-tree "+opt.work_tree;
	std::string call  = "/usr/bin/git "+gdir+wtree;
	std::string git_porcelain = exec(call+" status --porcelain --branch",0);

	GitParse result;
	std::string line;
	std::istringstream ss_result(git_porcelain);
	while (getline(ss_result, line, '\n')) {
		result.parse(line,call);
	}

	return result.str(opt);
}

int fileOlderSeconds(const std::string& fn) {
	struct stat result;
	if(stat(fn.c_str(), &result)==0) {
		auto mod_time = result.st_mtime;
		boost::posix_time::ptime when(boost::posix_time::from_time_t(mod_time)         );
		boost::posix_time::ptime now (boost::posix_time::second_clock::universal_time());

	// XXX I would use this one, but I prefer to avoid linking with boost_filesystem
	//	//std::time_t when = boost::filesystem::last_write_time(lock_2nd_fname);
	//         https://beta.boost.org/doc/libs/1_47_0/libs/filesystem/v3/doc/reference.html#last_write_time

		int older = (when-now).total_seconds(); // should be negative.
		if(older > 0) {
			return 1;
		}
		return older;
	};
	return 1;
}

void update_git_status(const std::string& lock_1st_fname, const std::string& lock_2nd_fname, const std::string& lock_3rd_fname, const Options& opt) {
	std::ofstream touch_file(lock_1st_fname);
	if(touch_file.is_open()) {
		touch_file.close();
	}
	// https://www.boost.org/doc/libs/1_68_0/doc/html/boost/interprocess/file_lock.html
	boost::interprocess::file_lock the_1st_lock(lock_1st_fname.c_str());
	if(the_1st_lock.try_lock()) {
		std::ofstream result_file(lock_2nd_fname);
		boost::interprocess::file_lock the_2nd_lock(lock_2nd_fname.c_str());
		if(the_2nd_lock.try_lock() and result_file.is_open()) {
			// the file result_file is open! We can store cached results there.
			std::string git_parsed_result = gitParsedResult(opt);
			result_file << git_parsed_result;
			result_file.close();
			the_2nd_lock.unlock();
			// maybe I am over-secure by using 3 files. But that is only to avoid any possible race condition.
			rename(lock_2nd_fname.c_str() , lock_3rd_fname.c_str());
		}
		// if cannoot write, then we better ignore that, because we have the result. So there is no reason to throw exception.
	}
}

int main(int argc, char** argv)
try {
	Options opt(argc,argv);
	if(opt.pwd_dir == "") throw ExecError("missing_pwd");//(101001);
	if(opt.whoami  == "") throw ExecError("missing_whoami");//(101005);
	std::string lockfile_name = sanitize(std::string("moj_git_status_PWD:"+opt.pwd_dir+"_WHO:"+opt.whoami+"_DIR:"+opt.git_dir+"_TREE:"+opt.work_tree));
	// maybe I am over-secure by using 3 files. But that is only to avoid any possible race condition.
	std::string lock_1st_fname = "/tmp/"+lockfile_name;
	std::string lock_2nd_fname = "/tmp/"+lockfile_name+"_RESULT";
	std::string lock_3rd_fname = "/tmp/"+lockfile_name+"_COPY";

	if(not opt.must_update_now) {
		int older = fileOlderSeconds(lock_3rd_fname);
		if(-older > opt.refresh_sec or older == 1) { // forced refresh
			// I wanted to use spawn_orphan (it's even in the git's history), to avoid blocking zsh. But I couldn't get this to work.
			// It only works outside zsh internal scripts.
			// https://stackoverflow.com/questions/17599096/how-to-spawn-child-processes-that-dont-die-with-parent
			// spawn_myself(opt,argv);
			update_git_status(lock_1st_fname,lock_2nd_fname,lock_3rd_fname,opt);
		}
		{
			// file locking failed, or we don't need to refresh. So let's return the conents of result_file
			std::ifstream result_file(lock_3rd_fname);
			if(result_file.is_open()) {
				std::string line;
				getline(result_file , line);
				std::cout << line << " " << older;
				return 0;
			} else {
				throw ExecError("cant_open_result_file");//(101002);
			}
		}
	} else {
		update_git_status(lock_1st_fname,lock_2nd_fname,lock_3rd_fname,opt);
	}


} catch(const ExecError& err) {
	if(err.code == 32768) { // fatal: Not a git repository - there is nothing to return. And actually not an error :)
		return 0;
	}
	if(err.msg.empty()) {
		std::cout << "err" << err.code << " 0 0 0 0 0 0 " << err.code;
	} else {
		std::cout << "error:"<< err.msg << " 0 0 0 0 0 0 " << err.code;
	}
	return err.code;
}


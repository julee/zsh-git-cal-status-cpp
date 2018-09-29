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

/* program:
 *
 * 1. zakłada unikalny lockfile(…) dla podanych argumentów + whoami + pwd
 *    → jesli uda się go założyć to wywołuje git status --porcelain --branch --git-dir ARG1 --work-tree ARG2
 *    Zwraca:
 *      [ branch , ahead , behind , conflicts , added , changed , untracked , kod_błędu ]
 *    Tylko gdy kod_błędu != 0, to reszta ma sens, w przeciwnym razie wskazuje co poszło źle w wywołaniu.
 *
 * 2. jeśli nie udało się założyć lockfile to:
 *    Zwraca wynik poprzedniego wywołania, jeśli taki istnieje
 *      [ branch , ahead , behind , conflicts , added , changed , untracked , kod_błędu ]
 *    W przeciwnym razie zwraca tylko kod błędu:
 *      [ branch ,   0   ,    0   ,     0     ,   0   ,    0    ,     0     , kod_błędu ]
 *
 * kody_błędów:
 *    0   : nie ma problemów
 *    101 : brakuje --pwd_dir
 */

#include "Options.hpp"

#include <stdexcept>
#include <sys/ioctl.h>
#include <iostream>
#include <fstream>
#include <boost/interprocess/sync/file_lock.hpp>

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

std::string exec(const std::string& cmd, int code) {
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

int main(int argc, char** argv)
try {
	struct winsize w; ioctl(0, TIOCGWINSZ, &w);
	Options opt(argc,argv,w.ws_col,15);
std::cerr << "pwd dir  : " << opt.pwd_dir   << "\n";
std::cerr << "git dir  : " << opt.git_dir   << "\n";
std::cerr << "work tree: " << opt.work_tree << "\n";
	if(opt.pwd_dir == "") throw ExecError(101);
	std::string whoami = exec("/usr/bin/whoami",1000);
std::cerr << "whoami   : \"" << whoami     << "\"\n";
	std::string lockfile_name = sanitize(std::string("moj_git_status_PWD:"+opt.pwd_dir+"_WHO:"+whoami+"_DIR:"+opt.git_dir+"_TREE:"+opt.work_tree));
std::cerr << lockfile_name << "\n";
	std::string lock_1st_fname = "/tmp/"+lockfile_name;
	std::string lock_2nd_fname = "/tmp/"+lockfile_name+"_RESULT";
	std::string touch  = exec("/usr/bin/touch "+lock_1st_fname,2000);
std::cerr << "touch    : \"" << touch      << "\"\n";

	boost::interprocess::file_lock the_1st_lock(lock_1st_fname.c_str());
	if(the_1st_lock.try_lock()) {
		std::ofstream result_file(lock_2nd_fname);
		boost::interprocess::file_lock the_2nd_lock(lock_2nd_fname.c_str());
		if(the_2nd_lock.try_lock()) {
			// mamy otwarty plik result_file, można do niego pisać.
		} else {
			// nie udało się do niego pisać, chociaż mamy wynik :(
		}
	} else {
		// nie udało się zakluczyć, zwracamy zawartość result_file
	}

} catch(const ExecError& err) {
	std::cout << "unknown 0 0 0 0 0 0 " << err.code;
}



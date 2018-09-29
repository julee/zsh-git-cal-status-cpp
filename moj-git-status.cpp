/* program:
 *
 * 1. zakłada unikalny lockfile(…) dla podanych argumentów + whoami + pwd
 *    → jesli uda się go założyć to wywołuje git status --porcelain --branch --git-dir ARG1 --work-tree ARG2
 *    Zwraca:
 *      [ kod_błędu , ahead , behind , conflicts , added , changed , untracked ]
 *    Tylko gdy kod_błędu != 0, to reszta ma sens, w przeciwnym razie wskazuje co poszło źle w wywołaniu.
 *
 * 2. jeśli nie udało się założyć lockfile to:
 *    Zwraca wynik poprzedniego wywołania, jeśli taki istnieje
 *      [ kod_błędu , ahead , behind , conflicts , added , changed , untracked ]
 *    W przeciwnym razie zwraca tylko kod błędu:
 *      [ kod_błędu ,   0   ,    0   ,     0     ,   0   ,    0    ,     0     ]
 *
 */

#include "Options.hpp"

#include <stdexcept>
#include <sys/ioctl.h>
#include <iostream>

struct Debug {
	template<typename Info>
	Debug& operator<<(const Info& info) {
		std::cout << info;
		return *this;
	}
};

Debug debug;

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

#include <sys/stat.h>
#include <sys/file.h>

int tryGetLock(const std::string lockName)
{
    mode_t m = umask( 0 );
    int fd = open( lockName.c_str(), O_RDWR | O_CREAT, 0666 );
    umask( m );
    printf("Opened the file. Press enter to continue...");
    fgetc(stdin);
    printf("Continuing by acquiring the lock.\n");
    if( fd >= 0 && flock( fd, LOCK_EX | LOCK_NB ) < 0 )
    {
        close( fd );
        fd = -1;
    }
    return fd;
}

int main(int argc, char** argv)
try {


	struct winsize w; ioctl(0, TIOCGWINSZ, &w);
	Options opt(argc,argv,w.ws_col,15);

	debug << "git dir  : " << opt.git_dir   << "\n";
	debug << "work tree: " << opt.work_tree << "\n";

	std::string pwd    = exec("pwd",1000);
	debug << "pwd      : \"" << pwd        << "\"\n";
	std::string whoami = exec("whoami",2000);
	debug << "whoami   : \"" << whoami     << "\"\n";

	std::string lockfile_name = sanitize(std::string("moj_git_status_PWD:"+pwd+"_WHO:"+whoami+"_DIR:"+opt.git_dir+"_TREE:"+opt.work_tree));
	debug << lockfile_name << "\n";

	//std::string cmd_test = exec("ls -laR /tmp",2000);
	//std::cout << "cmd_test  : \"" << cmd_test       << "\"\n";






} catch(const ExecError& err) {
	std::cout << "ExecError code: " << err.code << "\n";
}


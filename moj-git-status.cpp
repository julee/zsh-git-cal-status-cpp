#include "Options.hpp"

#include <stdexcept>
#include <sys/ioctl.h>
#include <iostream>

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

	std::cout << "git dir  : " << opt.git_dir   << "\n";
	std::cout << "work tree: " << opt.work_tree << "\n";

	std::string cmd_pwd = exec("pwd",1000);
	std::cout << "cmd_pwd  : \"" << cmd_pwd       << "\"\n";

	//std::string cmd_test = exec("ls -laR /tmp",2000);
	//std::cout << "cmd_test  : \"" << cmd_test       << "\"\n";
} catch(const ExecError& err) {
	std::cout << "ExecError code: " << err.code << "\n";
}


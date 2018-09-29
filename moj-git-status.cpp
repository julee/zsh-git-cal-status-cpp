#include "Options.hpp"

#include <sys/ioctl.h>
#include <iostream>

int main(int argc, char** argv)
{
	struct winsize w; ioctl(0, TIOCGWINSZ, &w);
	Options opt(argc,argv,w.ws_col,15);

	std::cout << "git dir  : " << opt.git_dir   << "\n";
	std::cout << "work tree: " << opt.work_tree << "\n";

}


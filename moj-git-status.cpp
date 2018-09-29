#include "Options.hpp"

#include <sys/ioctl.h>
#include <iostream>

int main(int argc, char** argv)
{
	struct winsize w; ioctl(0, TIOCGWINSZ, &w); Options opt(argc,argv,w.ws_col,15);

}


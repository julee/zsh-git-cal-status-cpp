all:
	g++ -Ofast git-status.cpp -o git-status.bin -std=c++14 -Wall -Wextra -Wpedantic -Wshadow -Wenum-compare -Wunreachable-code -Werror=narrowing -Werror=return-type -lboost_program_options
	g++ -Ofast git-cal.cpp -o git-cal.bin -std=c++14 -Wall -Wextra -Wpedantic -Wshadow -Wenum-compare -Wunreachable-code -Werror=narrowing -Werror=return-type -lboost_program_options
run:
	git-status

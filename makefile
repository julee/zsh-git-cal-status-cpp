all:
	#g++ szybki_test.cpp -o z -std=c++11
	#g++ szybki_test.cpp -o z -std=c++11 -I /home/salomea/yade/trunk -lfftw3f -Wall
	#g++ szybki_test.cpp -o z -std=c++11 -lCGAL -Wall -lmpfr -lgmp -I /usr/include/eigen3/
	#g++ szybki_test.cpp -o z -std=c++11 -Wall -O0
	#g++ szybki_test.cpp -o z  -lfftw3
	#g++ szybki_test.cpp -o z
	#-lboost_iostreams -lboost_serialization -lboost_program_options -lboost_system -lboost_filesystem -lboost_thread
	#-lboost_date_time -lstdc++ -lm -lQtCore -lQtGui -lpthread -lcpprest -lssl -lcrypto
	#g++ -O1 -g moj-git-status.cpp -o moj-git-status -std=c++17 -lreadline -lboost_system -lboost_thread -lboost_date_time -lpthread -lCGAL -Wall -lmpfr -lgmp -I /usr/include/eigen3/ \

	g++ -Ofast moj-git-status.cpp -o moj-git-status -std=c++14 -Wall -Wextra -Wpedantic -Wshadow -Wenum-compare -Wunreachable-code -Werror=narrowing -Werror=return-type
run:
	moj-git-status

#np_project2
CC := g++
boost_lib := /usr/local/boost_1_70_0
linker := -lboost_system -lpthread

all: console http sock

console: renderConsoleCGI.hpp renderConsoleCGI.cpp
	$(CC) -I $(boost_lib) $(linker) $^ -o hw4.cgi

http: http_server.cpp
	$(CC) -I $(boost_lib) $(linker) $^ -o http_server

sock: socks_server.cpp socks.hpp socks.cpp 
	$(CC) -I $(boost_lib) $(linker) $^ -o sock_server

test: test.cpp
	$(CC) -I $(boost_lib) $(linker) $^ -o test

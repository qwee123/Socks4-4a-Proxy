#np_project2
CC := g++
boost_lib := /usr/local/boost_1_70_0
linker := -lboost_system -lpthread

console: renderConsoleCGI.hpp renderConsoleCGI.cpp
	$(CC) -I $(boost_lib) $(linker) $^ -o console.cgi

sock: server.cpp socks.hpp socks.cpp 
	$(CC) -I $(boost_lib) $(linker) $^ -o sock_server


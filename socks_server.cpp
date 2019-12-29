#include <iostream>
#include <stdio.h>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <sys/wait.h>
#include <memory>
#include <utility>
#include <boost/algorithm/string/predicate.hpp>
#include "socks.hpp"
using namespace std;
using namespace boost::asio;

io_service global_io_service;

void signalHandler(int signo){
    if(SIGINT == signo){
        cout << "Exit" << endl;
        exit(0);
    }
    else if(SIGCHLD == signo){
        int pid,status;
        cout << "Terminate Child!" << endl;
        while((pid = waitpid(-1,&status,WNOHANG) > 0))    ;
    }
}

class SockServer{

    private:
         ip::tcp::acceptor _acc;
         ip::tcp::socket _sock;
         shared_ptr<SockSession> sess;

    public:
        SockServer(short port)
            : _acc(global_io_service, ip::tcp::endpoint(ip::tcp::v4(), port)),
              _sock(global_io_service) {
           do_accept();
        }

    private:
        void do_accept(){
            while(1){
                cout << "Wait Accept!" << endl;
                try{
                    _acc.accept(_sock);
                    cout << "Accept!" << endl;
                    handleAccept();
                }
                catch (const boost::system::error_code& err){
                    cerr << "Error on accept: " << err.message() << endl;
                }
            }
        }

        void handleAccept(){
           // SockSession sess(move(_sock));
            sess = make_shared<SockSession>(move(_sock));
            _sock.close();

            pid_t pid;
            while((pid = fork()) < 0){
                cerr << "Error: Cannot fork.....retry" << endl;
                sleep(1);
            }            

            if(pid == 0){
                sess->startService();
                exit(0);
            }
        }
};

int main(int argc, char* const argv[]){
    if(argc!=2) {
        cerr << "Usage:" << argv[0] << " [port]" << endl;
        return 1;
    }

    signal(SIGCHLD, signalHandler);
    signal(SIGINT, signalHandler);

    unsigned short port = atoi(argv[1]);
    SockServer server(port);
}

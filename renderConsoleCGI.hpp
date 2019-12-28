#ifndef RENDERCONSOLECGI_HPP
#define RENDERCONSOLECGI_HPP

#include <iostream>
#include <unistd.h>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <cstdlib>
#include <memory>
#include <fstream>
#include <vector>
using namespace std;
using namespace boost::asio;

string socks_host;
string socks_port;
void onSendConsoleHTML(const boost::system::error_code &, size_t);
void buildSession();
bool queryIsValid(vector<string>::iterator,
    vector<string>::iterator, vector<string>::iterator);

void renderHTML();
void renderStyle();
void renderSessionTable();

class RemoteSession: public enable_shared_from_this<RemoteSession> {
    private:
        int _sess_num;
        string host;
        unsigned int port;
        ifstream test_case;
        ip::tcp::socket _sock;
        ip::tcp::endpoint ep;
        ip::tcp::resolver _resolver;
        string res;
        string cmd;
        bool socks_enable;
        vector<unsigned char> socks_buf;

    public:
        RemoteSession(vector<string>::iterator,
              vector<string>::iterator, vector<string>::iterator,int );
        ~RemoteSession();
        string getSessionName();
        string getSessionID();
        void startService();

    private:
        void connectToServer();
        void socksInitiate();
        void onResolve(const boost::system::error_code &,ip::tcp::resolver::iterator);
        void onConnect(const boost::system::error_code &,ip::tcp::resolver::iterator);
        void onMesgSend(const boost::system::error_code &);
        void onMesgRecv(const boost::system::error_code &);
        void receiveFromServer();
        void sendToServer();
        void escapeResHTML();
        void escapeCmdHTML();
        void onOutputResult(const boost::system::error_code&);
};

vector<shared_ptr<RemoteSession>> sessions;

#endif

#ifndef SOCKS_HPP
#define SOCKS_HPP

#include <stdio.h>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <sys/wait.h>
#include <memory>
#include <utility>
#include <vector>
#include <boost/algorithm/string/predicate.hpp>
#include <fstream>

using namespace std;
using namespace boost::asio;

class SockSession: public enable_shared_from_this<SockSession> {
   
    private:
         ip::tcp::socket csock;
         ip::tcp::resolver resolver;
         ip::tcp::socket dstsock;
         ip::tcp::endpoint dst_ep;
         shared_ptr<ip::tcp::acceptor> dst_acc;
         vector<unsigned char> buf;                  
         vector<unsigned char> dst_buf; 
         uint8_t vn;
         uint8_t cd;
         uint16_t dst_port;
         vector<uint8_t> dst_ip;
         vector<unsigned char> userid;
         vector<unsigned char> domain_name;
         bool firewall_permission;
         int transmit_buf_size;

    public:
         SockSession(ip::tcp::socket socket);
         ~SockSession();
         void startService();

    private:
         void close();
         void parseRequest();
         void resolveRequest();
         void firewall();
         bool checkMode(const string&);
         bool checkIP(const string&, vector<string>*);
         void printReqMesg();
         void verifyFirewallDecision();
         void modeAction();
         void dstAccept();
         void connectToDest();
         void sendReply();
         void relayData();
};

#endif

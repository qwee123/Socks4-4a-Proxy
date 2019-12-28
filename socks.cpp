#include <iostream>
#include "socks.hpp"

extern io_service global_io_service;

SockSession::SockSession(ip::tcp::socket socket): csock(move(socket)), dstsock(global_io_service)
            , resolver(global_io_service) {
    cout << "Session Build!!" << endl;
    transmit_buf_size = 1024;
}

SockSession::~SockSession(){
    cout << "Session close" << endl;
}

void SockSession::close(){
    csock.close();
    dstsock.close();
    buf.clear();
    dst_buf.clear();
}

void SockSession::startService(){
    cout << "Start!" << endl;

    buf.resize(transmit_buf_size);
    try{
        csock.receive(buffer(buf));
    }
    catch (const boost::system::system_error& err){
        cerr << "Error on receive Socks4 request: " << err.what() << endl;
        exit(0);
    }

    if(buf[0] != 4){
        cerr << "Unknown VN number: " << +(uint8_t)buf[0] << endl;
        return ;
    }

    parseRequest();
    resolveRequest(); //build endpoint for later usage
    firewall(); //if deny, exit immediately
    printReqMesg();
    modeAction();
}

void SockSession::modeAction(){
    if(cd == 0x1){
        connectToDest();
        sendReply();
        cout << "Start Relaying...." << endl;
        relayData();
    }
    else if (cd == 0x2){
        dst_acc = make_shared<ip::tcp::acceptor>(global_io_service, ip::tcp::endpoint(ip::tcp::v4(), 0));
        sendReply();
        try{
            dst_acc->accept(dstsock);
        }
        catch(const boost::system::system_error& err){
            cerr << "Error on accept FTP server: " << err.what() << endl;
            exit(0);
        }
    }
    else    cerr << "Unknown CD number!" << endl;
}

void SockSession::firewall(){
    firewall_permission = true;

}

void SockSession::resolveRequest(){
    if(dst_ip[0] == 0 && dst_ip[1] == 0 && dst_ip[2] == 0 && dst_ip[3] > 0){
        if(domain_name.size() > 0){
            string hostname(domain_name.begin(), domain_name.end());
            ip::tcp::resolver::query query(hostname, to_string(dst_port));
            try{
                ip::tcp::resolver::iterator iter = resolver.resolve(query);
                dst_ep = *iter;
            }
            catch (const boost::system::system_error &err){
                cerr << "Error on resolver: " << err.what() << endl;
                this->close();
                exit(0);
            }
        }
        else {
            cerr << "Error request format!(No domainName is provide)" << endl;
            this->close();
            exit(0); 
       }
    }
    else{
        string ip_addr = to_string(dst_ip[0]) + "." + to_string(dst_ip[1]) + "."
            + to_string(dst_ip[2]) + "." + to_string(dst_ip[3]);
        dst_ep = ip::tcp::endpoint(ip::address::from_string(ip_addr), dst_port);
    }
}

void SockSession::connectToDest(){
    try {
        dstsock.connect(dst_ep);
    }
    catch (const boost::system::system_error &err){
        cerr << "Fail to Connect: " << err.what() << endl;
    }
}

void SockSession::sendReply(){
    vector<unsigned char> reply;
    reply.resize(8);

    reply[0] = 0x0;
    reply[1] = firewall_permission?90:91;
    if(firewall_permission && cd == 0x2 && dst_acc != NULL){  //for bind mode
        reply[2] = (uint8_t)(dst_acc->local_endpoint().port()>>8);
        reply[3] = (uint8_t)(dst_acc->local_endpoint().port() & 0xff);
        cout << +reply[2] << " " << +reply[3] << endl;
        reply[1] = 91;
    }
 
    try{ 
        csock.send(buffer(reply));
        if(reply[1] == 91){
            this->close();
            exit(0);
        }
    }
    catch (const boost::system::system_error& err){
        cerr << "Fail to sendReply: " << err.what() << endl; 
        exit(0);
    }
}

void SockSession::parseRequest(){
    auto itr = buf.begin();
    vn = *(itr++);
    cd = *(itr++);
    dst_port = ((uint16_t)(*(itr++))<<8) + (uint8_t)(*(itr++));
    dst_ip.reserve(4);
    dst_ip[0] = (uint8_t)(*(itr++));
    dst_ip[1] = (uint8_t)(*(itr++));
    dst_ip[2] = (uint8_t)(*(itr++));
    dst_ip[3] = (uint8_t)(*(itr++));

    for(;itr != buf.end()&&*itr != 0;itr++){
        userid.push_back((*itr));
    }

    if(*(++itr) == 0)    return;  

    for(;itr != buf.end()&&*itr != 0;itr++){
        domain_name.push_back((*itr));
    }
}

void SockSession::printReqMesg(){
    cout << "VN: " << +vn << endl;
    cout << "CD: " << +cd << endl;
    cout << "DSTPORT: " << dst_port << endl; 
    cout << "DSTIP: " << +dst_ip[0] << "." << +dst_ip[1] << "." << +dst_ip[2] << "." << +dst_ip[3] << endl;

    cout << "UserID: ";
    for(auto itr = userid.begin();itr != userid.end(); itr++){
        cout << *itr;
    }
    cout << endl;

    cout << "DomainName: ";
    for(auto itr = domain_name.begin();itr != domain_name.end();itr++){
        cout << *itr;
    }
    cout << endl;
}

void SockSession::relayData(){
    fd_set rfds, afds;
    int nfds;
    int csock_fd = csock.lowest_layer().native_handle();
    int dstsock_fd = dstsock.lowest_layer().native_handle();
 
    nfds = getdtablesize();
    FD_ZERO(&afds);
    FD_SET(csock_fd, &afds);
    FD_SET(dstsock_fd, &afds);

    while(1){
        memcpy(&rfds, &afds, sizeof(rfds));

        if(select((nfds), &rfds, NULL, NULL, NULL) < 0){
            cerr << "Error on select!" << endl;
            continue;
        }

        if(FD_ISSET(csock_fd, &rfds)){
            buf.clear();
            buf.resize(transmit_buf_size);
            try{
                size_t read_len = csock.receive(buffer(buf, transmit_buf_size));
                //cout << "Receive " << read_len << " bytes from client!" << endl;

                if(read_len == 0 || read_len < 0){
                    this->close();
                    cout << "RecvClient end!" << endl;
                    exit(0);
                }
                else{
                    boost::asio::write(dstsock, buffer(buf, read_len));
                }
            }
            catch (const boost::system::system_error& err){
                cerr << "Error on ClientToDest: " << err.what() << endl;
                this->close();
                exit(0);
            }
        }

        if(FD_ISSET(dstsock_fd, &rfds)){
            dst_buf.clear();
            dst_buf.resize(transmit_buf_size);
            try{
                size_t read_len = dstsock.receive(buffer(dst_buf, transmit_buf_size));
                //cout << "Receive " << read_len << " bytes from target!" << endl;

                if(read_len == 0 || read_len < 0){
                    this->close();
                    cout << "RecvTarget end!" << endl;
                    exit(0);
                }
                else{
                    boost::asio::write(csock,buffer(dst_buf,read_len));
                }
            }
            catch (const boost::system::system_error& err){
                cerr << "Error on DestToClient: " << err.what() << endl;
                this->close();
                exit(0);
            }
        }
    }
}

/*
void SockSession::clientToDest(){
    buf.clear();
    buf.resize(1024);
    auto self(shared_from_this());
    csock.async_receive(buffer(buf), [this, self](const boost::system::error_code& err, size_t rn){
        if(err){
            cerr << "Error on recvClient: " << err.message() << " " << rn << endl;
            this->close();
            return ;
        }
        global_io_service.post(strandDs.wrap(boost::bind(&SockSession::relayToDest, self, rn)));
    });
}

void SockSession::relayToDest(size_t rn){
    auto self(shared_from_this());
    dstsock.async_send(buffer(buf,rn), [this, self](const boost::system::error_code& err, size_t wn){
        if(err){
            cerr << "Error on sendDest: " << err.message() << endl;
            this->close();
            return ;
        }
        cout << "Send " << wn << " bytes to Dest" << endl;
        global_io_service.post(strandCs.wrap(boost::bind(&SockSession::clientToDest, self)));
    });
}

void SockSession::destToClient(){
    dst_buf.clear();
    dst_buf.resize(1024);
    auto self(shared_from_this());

    dstsock.async_receive(buffer(dst_buf), [this, self](const boost::system::error_code& err, size_t rn){
        if(err){
            cerr << "Error on recvDest: " << err.message() << " " << rn << endl;
            this->close();
            return ;
        }
        global_io_service.post(strandCs.wrap(boost::bind(&SockSession::relayToClient, self, rn)));
    });
}

void SockSession::relayToClient(size_t rn){
    auto self(shared_from_this());
    csock.async_send(buffer(dst_buf, rn), [this, self](const boost::system::error_code& err, size_t wn){
        if(err){
            cerr << "Error on sendClient: " << err.message() << endl;
            this->close();
            return ;
        }
        cout << "Send " << wn << " bytes to Client" << endl;
        global_io_service.post(strandDs.wrap(boost::bind(&SockSession::destToClient, self)));
    });
}
*/

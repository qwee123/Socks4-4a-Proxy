#include "renderConsoleCGI.hpp"
#define MAX_SERVERS 5
#define QUERY_TUPLE 3

io_service service;
posix::stream_descriptor out(service, ::dup(STDOUT_FILENO));
std::string console_html_content;

void on_output(const boost::system::error_code &ec, size_t bytes){
    if (ec)   cerr << "Error on output result" << endl;

}

void onSendConsoleHTML(const boost::system::error_code &ec, size_t bytes){
    if (ec){
	 cerr << "Error on send consoleCGI HTML" << endl;
         return ;
    }
 
    for(auto itr = sessions.begin();itr != sessions.end();itr++){
        (*itr)->startService();
    }
}

int main(int argc, char* const argv[]){

    buildSession();
    console_html_content.clear();
    renderHTML();
    out.async_write_some(buffer(console_html_content), onSendConsoleHTML);

    service.run(); 
    return 0;
}

void renderHTML(){
    console_html_content.reserve(9000);
    console_html_content = "Content-type:text/html\r\n\r\n";
    console_html_content.append("<!DOCTYPE html>\n");
    console_html_content.append("<html lang=\"en\">\n");
    console_html_content.append("<head>\n");
    renderStyle();
    console_html_content.append("</head>\n");

    console_html_content.append("<body>\n");
    renderSessionTable();
    console_html_content.append("</body>\n");
    console_html_content.append("</html>\n");
}

void renderStyle(){
    console_html_content.append("<meta charset=\"UTF-8\" />\n");
    console_html_content.append("<link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\" integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\" crossorigin=\"anonymous\" />\n");
    console_html_content.append("<link href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\" rel=\"stylesheet\" />\n");
    console_html_content.append("<link rel=\"icon\" type=\"image/png\" href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\" />\n");

    console_html_content.append("<style>\n");
    console_html_content.append("  * {\n");
    console_html_content.append("   font-family: 'Source Code Pro', monospace;\n");
    console_html_content.append("    font-size: 1rem !important;\n");
    console_html_content.append("  }\n");
    console_html_content.append("  body {\n");
    console_html_content.append("   background-color: #212529;\n");
    console_html_content.append("  }\n");
    console_html_content.append("  pre {\n");
    console_html_content.append("    color: #cccccc;\n");
    console_html_content.append("  }\n");
    console_html_content.append("  b {\n");
    console_html_content.append("    color: #ffffff;\n");
    console_html_content.append("  }\n");
    console_html_content.append(" </style>\n");
}

void renderSessionTable(){
    console_html_content.append("<table class=\"table table-dark table-bordered\">\n");
    console_html_content.append("<thead>\n");
    console_html_content.append("<tr>\n");
    for(auto itr = sessions.begin();itr != sessions.end(); itr++){
        console_html_content.append("<th scope=\"col\">" +  (*itr)->getSessionName() + "</th>\n");      
    }
    console_html_content.append("</tr>\n");
    console_html_content.append("</thead>\n");
    console_html_content.append("<tbody>\n");

    console_html_content.append("<tr>\n");
    for(auto itr = sessions.begin();itr != sessions.end(); itr++){
        console_html_content.append("<td><pre id=\"" + (*itr)->getSessionID() + "\" class=\"mb-0\"></pre></td>\n");
    }
    console_html_content.append("</tr>\n");
    
    console_html_content.append("</tbody>\n");
    console_html_content.append("</table>\n");
}

bool queryIsValid(vector<string>::iterator itr_h,
    vector<string>::iterator itr_p, vector<string>::iterator itr_tc){

    if(boost::algorithm::ends_with(*itr_h,"=")){
        cerr << "Invalid query value(host): " << *itr_h << endl;
        return false;
    }

    if(boost::algorithm::ends_with(*itr_p,"=")){
        cerr << "Invalid query value(port): " << *itr_p << endl;
        return false;
    }

    if(boost::algorithm::ends_with(*itr_tc,"=")){
        cerr << "Invalid query value(tc): " << *itr_tc << endl;
        return false;
    }
    return true;
}

void buildSession(){
    std::string query(getenv("QUERY_STRING"));       
    vector<string> v_query;
    v_query.reserve(MAX_SERVERS*QUERY_TUPLE);
    boost::algorithm::split(v_query,query,boost::is_any_of("&"));    

    int i = 0;
    auto itr = v_query.begin();
    for(;itr != v_query.end()-2;itr += QUERY_TUPLE){
        if(queryIsValid(itr,itr+1,itr+2)){ 
            sessions.push_back(move(make_shared<RemoteSession>(itr,itr+1,itr+2,i)));  
        }
        i++;      
    }

    auto pos = boost::algorithm::find_first(*itr,"=").begin();
    socks_host = (*itr).substr(pos - (*itr).begin() + 1);
    itr++;

    pos = boost::algorithm::find_first(*itr,"=").begin();
    socks_port = (*itr).substr(pos - (*itr).begin() + 1);
}

RemoteSession::RemoteSession(vector<string>::iterator h, vector<string>::iterator p,
        vector<string>::iterator tc,int sn):_sock(service),_resolver(service),_sess_num(sn){ 
 
    auto pos = boost::algorithm::find_first(*h,"=").begin(); 
    host = (*h).substr(pos - (*h).begin() + 1);

    pos = boost::algorithm::find_first(*p,"=").begin();
    port = stoul((*p).substr(pos - (*p).begin() + 1)); //might throw an error!

    pos = boost::algorithm::find_first(*tc,"=").begin();
    string filename = "./test_case/" + (*tc).substr(pos - (*tc).begin() + 1);
    if(!test_case)    test_case.close();
    test_case.open(filename);
    if(!test_case)    cerr << "Error: Cannot open file: " << filename << endl;
}

RemoteSession::~RemoteSession(){
    if(test_case)   test_case.close();
    _sock.close();

    res.clear();
    cmd.clear();
    cerr << "Session closed!" << endl;
}

string RemoteSession::getSessionName(){
    string name = host + ":" + to_string(port);
    return name;
}

string RemoteSession::getSessionID(){
    string SID = "s" + to_string(_sess_num);
    return SID;
}

void RemoteSession::startService(){
    connectToServer();
}

void RemoteSession::connectToServer(){
    if(socks_host.empty() || socks_port.empty()){
        socks_enable = false;
        ip::tcp::resolver::query query(host,to_string(port));
        _resolver.async_resolve(query,boost::bind(&RemoteSession::onResolve, this,
               boost::asio::placeholders::error, boost::asio::placeholders::iterator)); 
    }
    else{
        socks_enable = true;
        ip::tcp::resolver::query query(socks_host, socks_port);
        _resolver.async_resolve(query, boost::bind(&RemoteSession::onResolve, this,
               boost::asio::placeholders::error, boost::asio::placeholders::iterator));
    }  
}

void RemoteSession::onResolve(const boost::system::error_code& ec,ip::tcp::resolver::iterator ep_itr){
    if(ec){
        cerr << "Error: Fail to resolve DNS!" << endl;
        return ;
    }

    ep = *ep_itr;
     _sock.async_connect(ep,boost::bind(&RemoteSession::onConnect, 
           this, boost::asio::placeholders::error,++ep_itr));
}

void RemoteSession::onConnect(const boost::system::error_code& ec,ip::tcp::resolver::iterator ep_itr){
    if(!ec){
        if(socks_enable)    socksInitiate();
        else    receiveFromServer();
    }
    else if(ep_itr != ip::tcp::resolver::iterator()){ //try next endpoint
        _sock.close();
        ep = *ep_itr;
        _sock.async_connect(ep,boost::bind(&RemoteSession::onConnect,
            this, boost::asio::placeholders::error, ++ep_itr));
    }
    else{
        cerr << "Error: Fail to connect!" << endl;
        return ;
    }
}

void RemoteSession::socksInitiate(){
    socks_buf.resize(9);

    socks_buf[0] = 0x4; //version 4
    socks_buf[1] = 0x1; //connect mode
    socks_buf[2] = (uint8_t)(port >> 8);
    socks_buf[3] = (uint8_t)(port&0xff);
    socks_buf[4] = 0x0;//ip=0.0.0.1 ,apply socks4a
    socks_buf[5] = 0x0;
    socks_buf[6] = 0x0;
    socks_buf[7] = 0x1;   
    socks_buf[8] = 0x0;
    
    for(auto itr = host.begin();itr != host.end(); itr++)    socks_buf.push_back(*itr);

    socks_buf.push_back(0x0);
    auto self(shared_from_this());
    _sock.async_send(buffer(socks_buf, socks_buf.size()), [this, self]
           (const boost::system::error_code& err, size_t sn){
        if(err){
            cerr << "Error on send Socks request: " << err.message() << endl;
            return ;
        }        
  
        cerr << "after request" << endl; 
        socks_buf.clear();
        socks_buf.resize(8);
        _sock.async_receive(buffer(socks_buf, socks_buf.size()), [this, self]
                (const boost::system::error_code& err, size_t rn){
            if(err){    cerr << "Error on recv Socks reply: " << err.message() << endl;}
            else if(socks_buf[0] == 0x0 && socks_buf[1] == 90){ //pass
                cerr << "start revc" << endl;
                receiveFromServer();
            }
        });
    });
}

void RemoteSession::onMesgSend(const boost::system::error_code& ec){
    if(ec) cerr << "Error: Fail to send message" << endl;

    //receiveFromServer();
}

void RemoteSession::escapeResHTML(){
    boost::algorithm::replace_all(res,"\r\n","<br>");
    boost::algorithm::replace_all(res,"\n","<br>");
    boost::algorithm::replace_all(res,"\r","<br>");
    boost::algorithm::replace_all(res,"\'","\\'");
}

void RemoteSession::escapeCmdHTML(){
    boost::algorithm::replace_all(cmd,"\n","");
    boost::algorithm::replace_all(cmd,"\r","");
    boost::algorithm::replace_all(cmd,"\'","\\'");
}

void RemoteSession::onMesgRecv(const boost::system::error_code& ec){
    if(ec) {
        cerr << "Error: Fail to recv message" << endl; 
        return ;
    }

    escapeResHTML();//boost's find_first cannot find '\0' correctly...
    auto len = strlen(res.c_str());

    //Memory corruption will happen after the destructor is called if i try to resize,substr res here...    
    string trim_res; 
    trim_res.assign(res,0,len);

    string update_script = "<script>document.getElementById('" + getSessionID() + "').innerHTML += '" 
                      + trim_res + "';</script>\n";
    trim_res.clear();

    boost::asio::async_write(out, buffer(update_script), boost::bind(&RemoteSession::onOutputResult,
             this, boost::asio::placeholders::error));
 
    if(!boost::algorithm::find_last(res,"% ").empty()){ 
        sendToServer();
    }
    receiveFromServer();
}

void RemoteSession::receiveFromServer(){
    res.clear();
    res.resize(1024);
    _sock.async_receive(buffer(res),boost::bind(&RemoteSession::onMesgRecv,
             this, boost::asio::placeholders::error));
}

void RemoteSession::sendToServer(){
    cmd.clear();
    cmd.resize(15000);  
 
    getline(test_case,cmd);   
    
    escapeCmdHTML();
    //auto len = strlen(cmd.c_str());
    //string trim_cmd;
    //trim_cmd.assign(cmd,0,len);

    string update_script = "<script>document.getElementById('" + getSessionID() + "').innerHTML += '<b>"
                      + cmd + "<br></b>';</script>\n";
    //trim_cmd.clear();
    async_write(out, buffer(update_script),boost::bind(&RemoteSession::onOutputResult,
              this, boost::asio::placeholders::error));

    sleep(0.5);
    cmd.append("\n");
    _sock.async_send(buffer(cmd),boost::bind(&RemoteSession::onMesgSend,
                this, boost::asio::placeholders::error));

}

void RemoteSession::onOutputResult(const boost::system::error_code& ec){
    if(ec)    cerr << "Error on output result!" << endl;
}

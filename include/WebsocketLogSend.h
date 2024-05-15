//
// Created by zp on 18-12-24.
//

#ifndef L01_SDK_WEBSOCKETLOGSEND_H
#define L01_SDK_WEBSOCKETLOGSEND_H

#endif //L01_SDK_WEBSOCKETLOGSEND_H
#include <thread>
#include <mutex>
#include <list>
#include <set>
#include <string>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
typedef websocketpp::server<websocketpp::config::asio> WsServer;
typedef std::set<connection_hdl,std::owner_less<connection_hdl> > ConSet;
class WebsocketLogSend  {
public:
    WebsocketLogSend(){};
    virtual  ~WebsocketLogSend(){};
    virtual  void init()    ;
    virtual  void onMsg(connection_hdl hdl, WsServer::message_ptr msg);
    virtual  void onOpen(connection_hdl hdl);
    virtual  void onClose(connection_hdl hdl);
    virtual  void onInterrupt(connection_hdl hdl);
    virtual  void startLisenThread();
    virtual  void initWebsocket();
    void  saveLog(char* log);
private:
     void  sendLogThread();
     std::mutex inputCacheMutex;
     std::list<std::string> pbInputCache;
     WsServer wsServer;
     std::mutex hdlMutex;
     ConSet conSet;
     int interval;
};
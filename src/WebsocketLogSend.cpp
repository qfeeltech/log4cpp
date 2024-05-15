//
// Created by zp on 18-12-24.
//

#include "WebsocketLogSend.h"
#include <thread>
#define  MAX_INPUT_SIZE 100
void WebsocketLogSend::init() {
    initWebsocket();
    std::thread * pickThread = new std::thread(std::bind(&WebsocketLogSend::sendLogThread, this));
}
void WebsocketLogSend::saveLog(char* log)
{
    if(conSet.size() == 0) {
        //PRINT_LN;
        return;
    }
    {
        std::string str(log);
        std::lock_guard<std::mutex> lock( inputCacheMutex);
        if(pbInputCache.size() == MAX_INPUT_SIZE) {
            printf("WebsocketLogSend::saveLog reach max :%d\n",MAX_INPUT_SIZE);
            pbInputCache.pop_front();
        }
        pbInputCache.push_back(str);

    }

}
void WebsocketLogSend::sendLogThread()
{
    while (true)
    {
        //printf("con size %d\n",conSet.size());
        if(conSet.size() == 0||pbInputCache.size() == 0) {
            //PRINT_LN;
            usleep(1000*1000);
            continue;
        }
        std::string logstr ;
        {
            std::lock_guard<std::mutex> lock(inputCacheMutex);
            int cnt =0 ;
            for(auto it = pbInputCache.begin() ; it!= pbInputCache.end();++it)
            {
                logstr = logstr + *it + "<br/>";
                cnt++;
            }
            pbInputCache.clear();
        }
        for(ConSet::iterator itr = conSet.begin(); itr != conSet.end(); ) {
            connection_hdl hdl = *itr;

            websocketpp::lib::error_code errCode;
            wsServer.send(hdl, logstr, websocketpp::frame::opcode::text, errCode);

            if(errCode) {
                printf("getLogThread connection is broken, discard it\n");
                std::string reason ;
                websocketpp::lib::error_code errCloseCode;
                wsServer.close(*itr,websocketpp::close::status::going_away,reason,errCloseCode);
                printf("getLogThread close connection %d,%s",errCloseCode,reason.c_str());
                itr = conSet.erase(itr);
            } else {
                itr++;
            }
        }
        usleep(1000*500);
    }
}
void WebsocketLogSend::onMsg(connection_hdl hdl, WsServer::message_ptr msg) {
    printf("The command is from ip: %s", \
                  wsServer.get_con_from_hdl(hdl)->get_raw_socket().remote_endpoint().address().to_string().c_str());
}

void WebsocketLogSend::onOpen(connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(hdlMutex);
    printf("WebsocketServiceGeneric client conncted\n");
    conSet.insert(hdl);
}

void WebsocketLogSend::onClose(connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(hdlMutex);
    conSet.erase(hdl);
    printf("A WebsocketServiceGeneric client closed, still has %d \n", conSet.size());
    //LOG_ERROR("websocket client closed\n");
}

void WebsocketLogSend::onInterrupt(connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(hdlMutex);
    conSet.erase(hdl);
    printf("A WebsocketServiceGeneric client interrupted, still has %d connections\n", conSet.size());
}

void WebsocketLogSend::startLisenThread()
{
    try {
        wsServer.init_asio();
        wsServer.set_reuse_addr(true);
        wsServer.listen(boost::asio::ip::tcp::v4(), 9003);
        printf("\n================================\n");
        printf("   WebsocketServiceSend listen on 9003\n");
        printf("================================\n\n");
        wsServer.start_accept();
        while (1) {
            try {
                wsServer.run();
                break; // 正常退出
            }
            catch (std::exception e) {
                printf("WebsocketServiceSend catch websocket error :\n", e.what());
            }

        }
    }
    catch (std::exception e)
    {
        printf("WebsocketServiceGeneric catch websocket final error :\n", e.what());
    }
}
void WebsocketLogSend::initWebsocket()
{
    wsServer.set_access_channels(websocketpp::log::alevel::connect & websocketpp::log::alevel::disconnect);
    wsServer.clear_access_channels(websocketpp::log::alevel::frame_payload);
    wsServer.set_message_handler(bind(&WebsocketLogSend::onMsg, this, ::_1, ::_2));
    wsServer.set_open_handler(bind(&WebsocketLogSend::onOpen, this, ::_1));
    wsServer.set_close_handler(bind(&WebsocketLogSend::onClose, this, ::_1));
    wsServer.set_interrupt_handler(bind(&WebsocketLogSend::onInterrupt, this, ::_1));
    std::thread *inputThread = new std::thread(std::bind(&WebsocketLogSend::startLisenThread, this));
}

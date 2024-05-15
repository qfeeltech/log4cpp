/*******************************************************************************
* Copyright (C) 2014 Qfeeltech (Beijing) LTD,.
* All Rights Reserved
*
* The contents of this file are subject to the Mozilla Public License
* Version 2.0 (the "License"); you may not use this file except in
* compliance with the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS"
* basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
* License for the specific language governing rights and limitations
* under the License.
*
* File Name:    log_filter.cpp
* Abstract:     Module usage examples
* Author:      
* Version:      V1.0
* Finish Date:  2019/5/2
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boost/regex.hpp>
#include <cjson/cJSON.h>"
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#define FILE_NAME "/mnt/res/app/logFilter.json"
#include <fstream>
#include "WebsocketLogSend.h"
#include <signal.h>
#include <sys/msg.h>
#include <dirent.h>
#include <errno.h>
#include <iostream>

#define MSG_KEEPALIVE              1
#define MSG_REBOOT_NO_LOG          2
#define MSG_REBOOT_WITH_LOG        3
#define MSG_REBOOT_WITH_LOG_ACK    4
#define  LOG_FILE_NAME "logmain_log_file000.log"
#define  LOG_FILE_NAME_BAK "logmain_log_file000.log.1"
#define  LOG_MERGE_FILE_NAME "logmain_log_file000.log.last"
#define  IOT_FILE_NAME "iot.log"
#define  DMESG_FILE "dmesg.log"
#define VERSION  "V2.1_2021.11.24"
#define FILE_NUM_LIMIT 9

#include <log4cpp/Category.hh>
#include <log4cpp/Appender.hh>
#include <log4cpp/Priority.hh>
#include <unistd.h>
#include <log4cpp/RollingFileAppender.hh>
#include <log4cpp/PatternLayout.hh>
extern int errno;
#define  LOG_MAX_SIZE  2*1024*1024
#define  CORE_STEP_NEED  2
static int coreStep = 0 ;
std::string logPathDir;
log4cpp::RollingFileAppender *appender = NULL;
log4cpp::Category& logServer = log4cpp::Category::getInstance(std::string("QF"));
bool  hasEnableRule = false;
struct msg_st {
    long int msg_type;
    char text[128];
};
int msgid;
int nomsg_times = 0;
struct msg_st msg_rcv = {0};
struct msg_st msg_snd = {0};
//using namespace CommonUtils;
/*{
	"websocket_ip": "127.0.0.1",
	"websocket_port": "9002",
	"filter": [{
			"regex": "(\\w+)MST::(\\w+)",
			"audio_file": "clean_start.mp3 ",
			"pbinputBase64": "",
			"enable": 1
		},
		{
			"regex": "(\\w+)(\\w+)",
			"audio_file": "clean_resume.mp3 ",
			"pbinputBase64": "",
			"enable": 1
		}
	]
}
 */


typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// pull out the type of messages sent by our config
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;
static websocketpp::connection_hdl send_hdl;
client c;
// This message handler will be invoked once for each incoming message. It
// prints the message and then sends a copy of the message back to the server.
void SendCmd(std::string &pbinput)
{
    websocketpp::lib::error_code ec;
    c.send(send_hdl, pbinput.c_str(),pbinput.length(), websocketpp::frame::opcode::binary, ec);
    if (ec) {
        std::cout << "send failed because:" << ec.message() << std::endl;
    } else
    {
        printf(" send ok: pbinput :%s\n",pbinput.c_str());
    }
}
void on_message(client* c, websocketpp::connection_hdl hdl, message_ptr msg) {
//    std::cout << "logfilter on_message called with hdl: " << hdl.lock().get()
//              << " and message: " << msg->get_payload()
//              << std::endl;

    websocketpp::lib::error_code ec;
    send_hdl = hdl;
}
typedef  struct filterRuleSt{
    std::string regexstr;
    std::string pbInputBase64;
    bool        enable;
}*filterRule;
std::vector<filterRuleSt> ruleVec;
std::string ipstr="127.0.0.1",portstr="9002";
int max_size_config = LOG_MAX_SIZE ;
int file_num = 1;
int cache_size = 0;
bool upload_us  = false ;
int file_udisk_bak=3;
//std::deque<std::string> cache_log;
//std::mutex cache_mutex;
std::string upload_bucket="shark-us-va" ;
std::string upload_sn = "testSn";
WebsocketLogSend  *weblog = new WebsocketLogSend();
std::mutex upload_mutex;
std::vector<int> uploadPidList;
bool coreFlag = false;
int fileMaxIndex = 0 ;
std::string  getBakPath(const char * fileName)
{
    std::string fullPath ;
    char dirBase[128]={0};
    snprintf(dirBase,128,"/mnt/udisk/tmp/log%d/",fileMaxIndex+1);
    std::string dir = dirBase;
    fullPath = dir + fileName;
    return  fullPath;
}
std::string  getFullPath(const char * fileName)
{
    std::string fullPath ;
    if(fileName == (LOG_MERGE_FILE_NAME))
    {
        std::string dir = "/mnt/udisk/tmp/";
        fullPath = dir + fileName;
    } else{
        fullPath = logPathDir + fileName;
    }
    return  fullPath;
}
int runConsole(const char *cmd, std::vector<std::string> &resp)
{
    resp.clear();
    FILE* h =  popen(cmd, "r" );
    if (!h)
    {
        printf("\nERROR when running cmd: %s\n",cmd);
        return -1;
    }
    char tmp[1024]; //设置一个合适的长度，以存储每一行输出
    while (fgets(tmp, sizeof(tmp), h) != NULL) {
        if (tmp[strlen(tmp) - 1] == '\n') {
            tmp[strlen(tmp) - 1] = '\0'; //去除换行符
        }
        resp.push_back(tmp);
    }
    return pclose(h); //关闭管道
}
bool ReadFile()
{
    long len;
    char *pContent;
    int tmp;
    FILE *fp = fopen(FILE_NAME, "r+");
    if (!fp) {
        printf("%s can not find", FILE_NAME);
        return false;
    }
    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    if (0 == len) {
        printf("read len 0 error");
        return false;
    }

    fseek(fp, 0, SEEK_SET);
    pContent = (char *) malloc(sizeof(char) * len);
    tmp = fread(pContent, 1, len, fp);

    fclose(fp);
    cJSON *jsonRoot=NULL;
    jsonRoot = cJSON_Parse(pContent);
    //printf("content pContent %s", pContent);
    if (!jsonRoot) {
        printf("json error");
        return false;
    }
    try {
        cJSON *wbIpJson = cJSON_GetObjectItem(jsonRoot, "websocket_ip");
        if (wbIpJson != NULL) {
            ipstr = wbIpJson->valuestring;
        }
        cJSON *portJson = cJSON_GetObjectItem(jsonRoot, "websocket_port");
        if (portJson != NULL) {
            portstr = portJson->valuestring;
        }
        cJSON *logSizeJson = cJSON_GetObjectItem(jsonRoot, "rotate_size");
        if (portJson != NULL) {
            max_size_config = logSizeJson->valueint*1024;//单位k
        }
        cJSON *logDirJson = cJSON_GetObjectItem(jsonRoot, "log_path_dir");
        if (logDirJson != NULL) {
            logPathDir = logDirJson->valuestring;
            printf("logPathDir %s ,fullPath\n",logPathDir.c_str());
        }
        cJSON *logFileNumJson = cJSON_GetObjectItem(jsonRoot, "log_rotate_number");
        if (logFileNumJson != NULL) {
            file_num = logFileNumJson->valueint;
        }
        cJSON *logUploadUsJson = cJSON_GetObjectItem(jsonRoot, "upload_us_enble");
        if (logUploadUsJson != NULL) {
            upload_us = (logUploadUsJson->valueint== 1 )? true:false;
        }
        cJSON *logFileUdiskBakNumJson = cJSON_GetObjectItem(jsonRoot, "log_udisk_bak");
        if (logFileUdiskBakNumJson != NULL) {
            file_udisk_bak = logFileUdiskBakNumJson->valueint;
            printf("file_udisk_bak %d\n",file_udisk_bak);
        }
        cJSON *filter_arry = cJSON_GetObjectItem(jsonRoot, "filter");  //clientlist 是使用 cjson对象
        //printf("filter_arry ");
        if (NULL != filter_arry) {
            cJSON *client_list = filter_arry->child;
            while (client_list != NULL) {
                filterRuleSt rule;
                rule.regexstr = cJSON_GetObjectItem(client_list, "regex")->valuestring;
                rule.pbInputBase64 = cJSON_GetObjectItem(client_list, "pbinputBase64")->valuestring;
                rule.enable = cJSON_GetObjectItem(client_list, "enable")->valueint == 1? true:false;
                ruleVec.push_back(rule);
                printf("regex:%s \n",rule.regexstr.c_str());
                printf("pbInputBase64 %s\n",rule.pbInputBase64.c_str());
                printf("enable %d\n",rule.enable);
                client_list = client_list->next;
                if(rule.enable)
                {
                    hasEnableRule = true;
                }
            }
        }
    }
    catch (...)
    {
        printf("json try catch error");
    }
    free(pContent);
    cJSON_Delete(jsonRoot);
    return  true;
}
void saveLog(const std::string &log )
{
   if(log.find("[E]")!= std::string::npos)
   {
       logServer.error(log);
   }
   else if(log.find("[D]")!= std::string::npos)
    {
        logServer.debug(log);
    }
   else if(log.find("[W]")!= std::string::npos)
   {
       logServer.warn(log);
   }
   else  if(log.find("[F]")!= std::string::npos)
    {
        logServer.fatal(log);
    }
    else
    {
        logServer.info(log);
    }
}
void InitWebSocket(std::string ip,std::string port)
{
    std::string uri = "ws://"+ip+":"+port;
    printf("client InitWebSocket %s",uri.c_str() );

    try {
        // Set logging to be pretty verbose (everything except message payloads)
        c.set_access_channels(websocketpp::log::alevel::all);
        c.clear_access_channels(websocketpp::log::alevel::frame_payload);
        // Initialize ASIO
        //logwebsend already init;
        c.init_asio();
        // Register our message handler
        c.set_message_handler(bind(&on_message,&c,::_1,::_2));
        websocketpp::lib::error_code ec;
        client::connection_ptr con = c.get_connection(uri, ec);
        if (ec) {
            std::cout << "could not create connection because: " << ec.message() << std::endl;
            return ;
        }

        // Note that connect here only requests a connection. No network messages are
        // exchanged until the event loop starts running in the next line.
        c.connect(con);

        // Start the ASIO io_service run loop
        // this will cause a single connection to be made to the server. c.run()
        // will exit when this connection is closed.
        while (1) {
            try {
                c.run();
                break; // 正常退出
            }
            catch (std::exception e) {
                printf("WebsocketServiceGeneric catch websocket error :\n", e.what());
            }
        }
    } catch (websocketpp::exception const & e) {
        std::cout << e.what() << std::endl;
    }
};
void RegexFunc(std::string temp)
{
    if(!hasEnableRule)
    {
        return;
    }
    auto it =  ruleVec.begin();
    for(it;it!=ruleVec.end();it++)
    {
        boost::regex reg(it->regexstr);
        if(it->enable&&boost::regex_search(temp, reg))
        {
            printf("regex:%s,match ok\n",it->regexstr.c_str());
            saveLog("logFilter regex match ok");
            SendCmd(it->pbInputBase64);
        } else{
        }
    }
}
//callback调用不能用saveLog;
bool mergeFile()
{
    printf("begin merge file");
    bool retFlag = false;
    if(access(getFullPath(LOG_FILE_NAME).c_str(), 0) != -1)
    {
        if(access(getFullPath(DMESG_FILE).c_str(), 0) != -1)
        {
            char cmd[128]={0};
            snprintf(cmd,128,"cat  %s >> %s",getFullPath(DMESG_FILE).c_str(),getFullPath(LOG_FILE_NAME).c_str());
            int ret = system(cmd);
            if(ret == 0)
            {
                printf("attach dmesg ok ");
            } else
            {
                printf("attach dmesg ok error ");
            }
        }
        std::string concatFile;
        //sort file
        std::vector<std::string> files;
        char cmd[512]={0};
        snprintf(cmd,512,"ls  -1tr %s* ",getFullPath(LOG_FILE_NAME).c_str());
        int ret = runConsole(cmd, files);
        int file_num=1;
        for( std::vector<std::string>::iterator it = files.begin(); it != files.end(); it++,file_num++)
        {
            if(it->rfind(".last")!=std::string::npos )
            {
                file_num--;
                continue;
            }
            if(file_num>FILE_NUM_LIMIT)
            {
                printf("reach file num max %d",FILE_NUM_LIMIT);
                break;
            }
            //char file[56]={'\0'};
            //sprintf(file,"%s.%d",LOG_FILE_NAME,file_num);
            if(access(it->c_str(), 0) !=-1)
            {
                std::string fileaname=it->c_str();
                concatFile=concatFile+" "+fileaname;
                printf("found %s\n",it->c_str());
            }
            else
            {
                printf("not found %s\n",it->c_str());
            }
        }
        if(concatFile !="")
        {
            char cmd[512]={0};
            snprintf(cmd,512,"cat %s > %s",concatFile.c_str(),getFullPath(LOG_MERGE_FILE_NAME).c_str());
            int ret = system(cmd);
            printf("cp cmd: %s\n",cmd);
            if(ret == 0)
            {
                printf("logFilter merge file  ok ");
                retFlag =  true;
            } else
            {
                printf("logFilter merge  file error ");
            }

        }
        else
        {
            printf("concat file none ");
        }
    } else
    {
        printf("logFilter log file not found ");
    }
    return  retFlag;
}
//void saveToFile()
//{
//    printf("save log to file begin cache size: %d\n ",cache_size);
//    std::ofstream in;
//    std::lock_guard<std::mutex> tempUpload(upload_mutex);
//    in.open(LOG_FILE_NAME,std::ios::trunc); //ios::trunc表示在打开文件前将文件清空,由于是写入,文件不存在则创建
//    if(in.is_open()) {
//        //std::string *writeStr = new std::string() ;
//        {
//            std::lock_guard<std::mutex> temp(cache_mutex);
//            auto it = cache_log.begin();
//            for (it; it != cache_log.end(); it++) {
//                //in << *it << "\n";
//                //*writeStr = *writeStr + *it + "\n";
//                in.write(it->c_str(),it->length());
//                in.write("\n",1);
//            }
//        }
//        //in <<  *writeStr;
//        //in.write(writeStr->c_str(),writeStr->length());
//        in.close();//关闭文件
//
//        //delete writeStr;
//        //writeStr = NULL;
//    } else
//    {
//        std::lock_guard<std::mutex> tempMutex(cache_mutex);
//        cache_log.push_back("logFilter ofstream can not open");
//        cache_size++;
//    }
//    printf("save log to file end size:%d\n ",cache_size);
//}
float getSysTime()
{
    struct timespec sTimeStamp;
    clock_gettime(CLOCK_MONOTONIC, &sTimeStamp);
    return ((float)(sTimeStamp.tv_sec + (sTimeStamp.tv_nsec * 1e-9)));
}
void getMacAddress(std::string& mac)
{
    const char * cmd = "cat /sys/class/net/wlan0/address";
    std::vector<std::string> resp;
    runConsole(cmd, resp);
    if (resp.size() > 0) {
        mac = resp[0];
    }

}
void  proccessExistWithPID(const char * fileName)
{
    char cmd[128]={0};
    snprintf(cmd,128,"ps -ef |grep %s |grep -v grep | busybox awk '{print $1}'",fileName);
    std::vector<std::string> retStr;
    int ret = runConsole(cmd,retStr);
    if(ret == 0)
    {
        for(int i = 0 ; i < retStr.size() ;i++)
        {
            try{
                uploadPidList.push_back(atoi(retStr[i].c_str()));
            }
            catch(...)
            {
                printf("atoi error");
            }
        }
    }
}
//回调步骤不允许调用saveLog
void excuteAndAck(const std::string &cmd,bool needAck,bool needSaveCmd)
{
    if(needSaveCmd)
    {
        char flagCmd[128] = {0};
        snprintf(flagCmd, 128,"echo \"%s\" > /mnt/udisk/tmp/log%d/upload.cmd", cmd.c_str(), fileMaxIndex+1);
        system(flagCmd);
    }
    proccessExistWithPID("uploadFile") ;
    for(int i = 0 ;i < uploadPidList.size() ; i ++)
    {
        char cmdkill[56]={0};
        snprintf(cmdkill,56,"kill -9 %d ",uploadPidList[i]);
        std::vector<std::string> retStr;
        int  ret = runConsole(cmdkill,retStr);
        if(ret !=0 )
        {
            printf("kill error pid %d\n",uploadPidList[i]);
        } else
        {
            printf("kill pid success %d\n",uploadPidList[i]);
        }
    }
    uploadPidList.shrink_to_fit();
    //excute
    if(upload_bucket == "shark-bj" || upload_us) {
        printf("###excuteAndAck %s needAck %d  \n",cmd.c_str(),needAck);
        char cmdExcute[512] = {0};
        snprintf(cmdExcute, 512,"/mnt/res/app/uploadFile %s ", cmd.c_str());
        std::vector<std::string> retStr;
        int ret = runConsole(cmdExcute, retStr);
        if (ret != 0) {
            printf("excuteE error %d", ret);
        }
    }
    if(needAck) {
        printf("logFilter logAck\n");
        msg_snd.msg_type = MSG_REBOOT_WITH_LOG_ACK;
        strncpy(msg_snd.text, "upload log before reboot", 128);
        msgsnd(msgid, (void *) &msg_snd, sizeof(struct msg_st) - sizeof(long),
               0);
    }
}
void uploadLog(const std::string &fileNameSrc,const std::string &bucket,const std::string & sn,bool needAck )
{
    //find uploadProcess()
    std::string cmd = fileNameSrc + "#" + bucket + "#" + sn;
    excuteAndAck(cmd,needAck,true);
    return;



    //readFile
//    try {
//        saveLog("logFilter begin upload: " + fileNameSrc);
//        std::lock_guard<std::mutex> tempUpload(upload_mutex);
//        if (fileNameSrc == getFullPath(LOG_MERGE_FILE_NAME) && !mergeFile()) {
//            return;
//        }
//        std::string fileName = fileNameSrc;
//        std::string macAddress;
//        getMacAddress(macAddress);
//        std::string macPoster = macAddress.substr(macAddress.length() - 5, 5);
//        macPoster.erase(std::remove(macPoster.begin(), macPoster.end(), ':'), macPoster.end());
//        std::string copyname = fileName + "." + macPoster;
//        printf("begin upload file %s ,rename :%s\n", fileName.c_str(), copyname.c_str());
//        FILE *fptr;
//        struct curl_slist *http_header = NULL;
//        //upload
//        CURL *curl;
//        CURLcode res;
//        //curl_global_init(CURL_GLOBAL_DEFAULT); move to main
//        saveLog("logFilter init begin");
//        curl = curl_easy_init();
//        saveLog("logFilter init finish");
//        if (curl) {
//            if (bucket == "shark-bj") {
//                curl_easy_setopt(curl, CURLOPT_URL, POSTURL_CN);
//            } else {
//                curl_easy_setopt(curl, CURLOPT_URL, POSTURL);
//            }
//            curl_easy_setopt(curl, CURLOPT_POST, 1);
//            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
//            curl_easy_setopt(curl, CURLOPT_HEADER, 1);
//            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
//            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
//            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
//            //每秒10字节丢弃
//            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT , 10);
//            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME , 5);
//
//            struct curl_httppost *formpost = 0;
//            struct curl_httppost *lastptr = 0;
//            curl_formadd(&formpost, &lastptr, CURLFORM_PTRNAME, "reqformat", CURLFORM_PTRCONTENTS, "plain",
//                         CURLFORM_END);
//            curl_formadd(&formpost, &lastptr, CURLFORM_PTRNAME, "time", CURLFORM_PTRCONTENTS, "1557899400",
//                         CURLFORM_END);
//            curl_formadd(&formpost, &lastptr, CURLFORM_PTRNAME, "code", CURLFORM_PTRCONTENTS, POST_CODE, CURLFORM_END);
//            curl_formadd(&formpost, &lastptr, CURLFORM_PTRNAME, "sn", CURLFORM_PTRCONTENTS, sn.c_str(), CURLFORM_END);
//            if (bucket != "") {
//                curl_formadd(&formpost, &lastptr, CURLFORM_PTRNAME, "bucket", CURLFORM_PTRCONTENTS, bucket.c_str(),
//                             CURLFORM_END);
//            }
//            curl_formadd(&formpost, &lastptr, CURLFORM_PTRNAME, "file", CURLFORM_FILE, fileName.c_str(),
//                         CURLFORM_FILENAME, copyname.c_str(), CURLFORM_END);
//            curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
//            saveLog("logFilter upload curl_easy_perform() start ");
//            res = curl_easy_perform(curl);
//            /* Check for errors */
//            if (res != CURLE_OK) {
//                printf("upload curl_easy_perform() failed : %s\n",
//                       curl_easy_strerror(res));
//                {
//                    saveLog("logFilter upload curl_easy_perform() failed ");
//                }
//                int res_sec = curl_easy_perform(curl);
//                if (res_sec != CURLE_OK) {
//                    saveLog("logFilter upload curl_easy_perform() failed again");
//                } else {
//                    saveLog("logFilter upload curl_easy_perform() success in the second time");
//                }
//            } else {
//                saveLog("logFilter upload curl_easy_perform() success");
//                printf("upload curl_easy_perform() success");
//            }
//
//            /* always cleanup */
//            curl_easy_cleanup(curl);
//        } else {
//            saveLog("logFilter curl null ");
//        }
//        saveLog("logFilter curl_easy_cleanup ");
//        //curl_global_cleanup();
//        if (needAck) {
//            //会被重启
//            coreStep++;
//            if (coreStep == CORE_STEP_NEED) {
//                saveLog("logFilter curl_global_cleanup");
//                curl_global_cleanup();
//                saveLog("logFilter logAck");
//                msg_snd.msg_type = MSG_REBOOT_WITH_LOG_ACK;
//                strncpy(msg_snd.text, "upload log before reboot", 128);
//                msgsnd(msgid, (void *) &msg_snd, sizeof(struct msg_st) - sizeof(long),
//                       0);
//                saveLog("logFilter send end ");
//            }
//        }
//    }
//    catch (...)
//    {
//        saveLog(strerror(errno));
//        saveLog("upload catch error");
//    }

}
int findAllSubDir(std::vector<std::string> &filelist, const char *basePath)
{
    DIR *dir;
    struct dirent *ptr;
    char base[1000];

    if ((dir=opendir(basePath)) == NULL)
    {
        perror("Open dir error...");
        return  -1;
    }

    while ((ptr=readdir(dir)) != NULL)
    {
        if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0)    ///current dir OR parrent dir
            continue;
        else if(ptr->d_type == 8)    //file
        {
            // //printf("d_name:%s/%s\n",basePath,ptr->d_name);
            // string temp = ptr->d_name;
            // //cout  << temp << endl;
            // string sub = temp.substr(temp.length() - 4, temp.length()-1);
            // //cout  << sub << endl;
            // if(sub == format)
            // {
            //     string path = basePath;
            //     path += "/";
            //     path += ptr->d_name;
            //     filelist.push_back(path);
            // }
        }
        else if(ptr->d_type == 10)    ///link file
        {
            //printf("d_name:%s/%s\n",basePath,ptr->d_name);
        }
        else if(ptr->d_type == 4)    ///dir
        {
            memset(base,'\0',sizeof(base));
            strcpy(base,basePath);
            strcat(base,"/");
            strcat(base,ptr->d_name);
            filelist.push_back(ptr->d_name);
            findAllSubDir(filelist, base);
        }
    }
    closedir(dir);
    return 0;
}
//callback 不能调savelog
void cpLog()
{
   printf("zp##########1");
   if(access("/mnt/udisk/tmp/", 0) == -1)
   {
       system("mkdir /mnt/udisk/tmp/");
   }
   std::string dir = "/mnt/udisk/tmp/";
   std::vector<std::string> dirlist;
   int fileMinIndex = 99999 ;
   fileMaxIndex = 0 ;
   std::string dirNew;
   std::vector<int> removeList ;
   int count = 0;
   if(findAllSubDir(dirlist,"/mnt/udisk/tmp/") == 0)
   {
       auto  it = dirlist.begin();
       for(it ;it!= dirlist.end(); ++ it)
       {
            int index =1;
            int right=sscanf(it->c_str(),"log%d",&index);
            if(right==1)
            {
                count++;
                if(index > fileMaxIndex)
                {
                    fileMaxIndex = index;
                }
                if(index < fileMinIndex)
                {
                    fileMinIndex = index;
                }
            }
       }
       it = dirlist.begin();
       int indexCnt = fileMaxIndex-fileMinIndex;
       int removeMax= (indexCnt+2 - file_udisk_bak)? (indexCnt+2 - file_udisk_bak):0;
       for(it ;it!= dirlist.end(); ++ it)
       {
           int index = 1;
           int right = sscanf(it->c_str(),"log%d",&index);
           if(right==1 && index <fileMinIndex + removeMax )
           {
               removeList.push_back(index);

           }
       }
   }
   char cmd[512]={0};
   char cmdRm[128]={0};
   printf("count %d,file_udisk_bak:%d\n",count,file_udisk_bak);
   if(count < file_udisk_bak)
   {
       snprintf(cmd,512,"mkdir /mnt/udisk/tmp/log%d; cp -f /tmp/logmain* /mnt/udisk/tmp/logmain_log_file000.log.last /tmp/AppInfo.txt  /tmp/dmesg.log  /tmp/wpa_supplicant.config /mnt/udisk/tmp/log%d;",fileMaxIndex+1,fileMaxIndex+1);
       //sprintf(cmd,"cp -af /tmp/ /mnt/udisk/tmp/log%d; sync;",fileMaxIndex+1);
       system(cmd);
   } else
   {
       for(auto it = removeList.begin();it != removeList.end(); it++)
       {
           printf("delete log%d\n",*it);
           snprintf(cmdRm,128,"rm -rf /mnt/udisk/tmp/log%d",*it);
           system(cmdRm);
       }
       removeList.shrink_to_fit();
       char cmd1[512]={0};
       //sprintf(cmd1,"cp -af /tmp/ /mnt/udisk/tmp/log%d; sync",fileMaxIndex+1);
       snprintf(cmd1,512,"mkdir /mnt/udisk/tmp/log%d;cp -f /tmp/logmain* /mnt/udisk/tmp/logmain_log_file000.log.last /tmp/AppInfo.txt   /tmp/dmesg.log  /tmp/wpa_supplicant.config /mnt/udisk/tmp/log%d",fileMaxIndex+1,fileMaxIndex+1);
       system(cmd1);
       usleep(50*1000);
   }
    system("sync");
}
void rollOverCallback()
{
    cpLog();
    std::thread  *pUploadLog = new std::thread(std::bind(uploadLog, getBakPath(LOG_FILE_NAME_BAK),upload_bucket,upload_sn, false));
    pUploadLog->detach();
    delete pUploadLog;
}
void watchProcess()
{
    while(1) {

        if (msgrcv(msgid, (void *)&msg_rcv, sizeof(struct msg_st) - sizeof(long),
                   MSG_REBOOT_WITH_LOG, IPC_NOWAIT | MSG_NOERROR) > 0)
        {
            nomsg_times ++;

            {
                printf("watchProcess roboeye core dump or no data received for a long time");
                saveLog("watchProcess roboeye core dump or no data received for a long time");
            }
            coreFlag = true;
            std::vector<std::string> resp;
            printf("watchProcess roboeye core dump or no data received for a long time\n");
            const char *cmd1 = "dmesg";
            int ret = runConsole(cmd1,resp);
            if(resp.size() > 0)
            {
                printf("watchProcess save file\n");
                std::ofstream in;
                in.open(getFullPath(DMESG_FILE),std::ios::trunc); //ios::trunc表示在打开文件前将文件清空,由于是写入,文件不存在则创建
                auto it = resp.begin();
                for(it;it!= resp.end(); it++)
                {
                    in<<*it<<"\n";
                }
                coreStep = 0;
                in.close();//关闭文件
                char cmd[256] ={'\0'};
                snprintf(cmd,256,"ps >> %s",getFullPath(DMESG_FILE).c_str());
                system(cmd);
                char cmd1[256] ={'\0'};
                snprintf(cmd1,256,"busybox free  >> %s",getFullPath(DMESG_FILE).c_str());
                system(cmd1);
                char cmd2[256] ={'\0'};
                snprintf(cmd2,256,"cat /proc/interrupts >> %s",getFullPath(DMESG_FILE).c_str());
                system(cmd2);

                if (!mergeFile()) {
                    printf("mergeFile error");
                }
                cpLog();
//                std::thread  *pUploadMesg = new std::thread(std::bind(uploadLog, getFullPath(DMESG_FILE),upload_bucket,upload_sn,true));
//                pUploadMesg->detach();
//                delete pUploadMesg;
//                std::thread  *pUploadLog = new std::thread(std::bind(uploadLog, getFullPath(LOG_MERGE_FILE_NAME),upload_bucket,upload_sn,true));
//                pUploadLog->detach();
//                delete pUploadLog;


                //fileNameSrc#bucket#sn,fileNameSrc1#bukket1#sn1;
                std::string excuteCmd  = getBakPath(DMESG_FILE) + "#" + upload_bucket + "#" + upload_sn + ","+ getBakPath(LOG_MERGE_FILE_NAME) + "#" + upload_bucket + "#" + upload_sn ;
                std::thread  *pUploadLog = new std::thread(std::bind(excuteAndAck, excuteCmd,true, true));
                pUploadLog->detach();
                delete pUploadLog;
            }
        }
        sleep(1);
    }
}
//void watchFunc()
//{
//    while(!coreFlag)
//    {
//        watchProcess();
//        sleep(10);
//    }
//}
void checkSpeciallog(const std::string & temp)
{
    //test ;
    if(temp.find("[logAgent][uploadLog]")!= std::string::npos)
    {
        saveLog("start uploadLog\n");
        if (!mergeFile()) {
            printf("mergeFile error");
        }
        cpLog();
        std::thread  *pUpload = new std::thread(std::bind(uploadLog, getBakPath(LOG_MERGE_FILE_NAME),upload_bucket,upload_sn, false));
        pUpload->detach();
        delete pUpload;
    }
    else if(temp.find("[logAgent][bucket]")!= std::string::npos)
    {
        printf("found bucket\n");
        int loc = temp.find("--");
        upload_bucket = temp.substr(loc+2,temp.length()-loc);
        printf("get bucket:%s\n",upload_bucket.c_str());
    }
    else if(temp.find("[logAgent][sn]")!= std::string::npos)
    {
        printf("found sn\n");
        int loc = temp.find("--");
        upload_sn = temp.substr(loc+2,temp.length()-loc);
        printf("get sn:%s\n",upload_sn.c_str());
    }
//    else if(temp.find("[logAgent][iot]")!= std::string::npos)
//    {
//        printf("found iot\n");
//        std::string iot  ;
//        int loc = temp.find("--");
//        iot = temp.substr(loc+2,temp.length()-loc);
//        printf("get iot:%s\n",iot.c_str());
//        std::ofstream in;
//        in.open(IOT_FILE_NAME,std::ios::trunc); //ios::trunc表示在打开文件前将文件清空,由于是写入,文件不存在则创建
//        in<<iot;
//        in.close();//关闭文件
//    }
    else if(temp.find("[logAgent][syncTime]")!= std::string::npos)
    {
        printf("found syncTime\n");
        //upload delay
        //std::thread  *pUpload = new std::thread(std::bind(uploadLog, getFullPath(IOT_FILE_NAME),upload_bucket,upload_sn, false));
        //printf("upload iot\n");
    }
    else if(temp.find("[logAgent][logWebSend]")!= std::string::npos)
    {
        printf("found  logWebsend\n");
        weblog->init();
    }
    else if(temp.find("[logAgent][websocketServer]")!= std::string::npos)
    {
        printf("found websocketServer\n");
        std::thread *intThread = new std::thread(std::bind(InitWebSocket,ipstr,portstr));
    }

}
void testCp()
{

    while(true)
    {
        cpLog();
        sleep(5);
    }
}
void rebootUpload()
{
    std::string fullCmd="";
    std::vector<std::string> dirlist;
    std::string parent = "/mnt/udisk/tmp/";
    printf("rebootUpload\n");
    if(findAllSubDir(dirlist,parent.c_str()) == 0)
    {
        auto  it = dirlist.begin();
        for(it ;it!= dirlist.end(); ++ it)
        {
            std::string file1 = parent + it->c_str() + "/" + LOG_FILE_NAME_BAK +".ok";
            std::string file2 = parent + it->c_str() + "/" + LOG_MERGE_FILE_NAME +".ok";
            std::string file3 = parent + it->c_str() + "/" + DMESG_FILE +".ok";
            if(access(file1.c_str(), 0) == -1 &&
               access(file2.c_str(), 0) == -1 &&
               access(file3.c_str(), 0) == -1 )
            {
                std::vector<std::string> resp;
                char cmd1[512]={0};
                snprintf(cmd1,512,"cat /mnt/udisk/tmp/%s/upload.cmd ",it->c_str());
                printf("search cmd %s\n",cmd1);
                int ret = runConsole(cmd1,resp);
                if(ret == 0 &&resp.size() > 0) {
                    if (resp[0] != "") {
                        if (fullCmd != "") {
                            fullCmd = fullCmd + "," + resp[0];
                        } else {
                            fullCmd = resp[0];
                        }
                    }
                }
            }
        }
    }
    if(fullCmd!="") {
        std::thread  *pUploadLog = new std::thread(std::bind(excuteAndAck, fullCmd, false, false));
        pUploadLog->detach();
        delete pUploadLog;
        printf("reboot need upload full cmd %s", fullCmd.c_str());
    }
}
static void usage(void)
{
    fprintf(stderr,
            "%s\n"
            "Usage: %s -v\n"
            "    -v <version>         n"
        );
}

static void gen_opts(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "v")) != -1) {
        switch (opt) {
            case 'v':
                 printf("%s\n",VERSION);
                 exit(0);
            default:
                usage();
                break;
        }
    }
}
int main(int argc, char **argv)
{

    gen_opts(argc, argv);
    //curl_global_init(CURL_GLOBAL_DEFAULT);
    rebootUpload();
    ReadFile();
    //read json param,log regex,music file,pbinput(base64).
    //init socket;
    //std::thread *intThread = new std::thread(std::bind(InitWebSocket,ipstr,portstr));
    //websocket server
    //WebsocketLogSend  *weblog = new WebsocketLogSend();
    //weblog->init();
    //todo upload log ,manual ,clean finish auto,oss info auto
    //todo watch rs ,upload dmesg
    appender =  new log4cpp::RollingFileAppender("default", getFullPath(LOG_FILE_NAME),LOG_MAX_SIZE,file_num);
    appender->setMaximumFileSize(max_size_config);
    int size = appender->getMaxFileSize();
    printf("config size %d,log rotate size %d\n",size);

    log4cpp::PatternLayout *fileLayout = new log4cpp::PatternLayout();
    //默认fileLayout->setConversionPattern("%m %n");
    fileLayout->setConversionPattern("%R%m %n");
    appender->setLayout(fileLayout);
    logServer.addAppender(appender);
    logServer.setPriority(log4cpp::Priority::DEBUG);
    //appender->setRollOverBeforeCallback(cpLog);
    appender->setRollOverAfterCallback(rollOverCallback);
    msgid = msgget((key_t)1234, 0666);
    if (msgid == -1) {
        perror("logFilter msd error");
    }
    else
    {
        std::thread *watchThread = new std::thread(watchProcess);
        printf("logFilter get msgId :%ld\n",msgid);
    }
    struct timeval tv;
    float intervalSave = getSysTime();
    while(true)
    {
//        //debug
//        printf("begin");
//        mergeFile();
//        printf("end");
//        std::fflush(stdout);
//        sleep(10);
//        continue;
//        //end
        std::string temp;
        std::getline(std::cin,temp);
        if(temp.length()!= 0)//have log
         {
             //temp = "test str";
             weblog->saveLog(temp.c_str());
             cache_size ++ ;
             //match regex fuction
             RegexFunc(temp);
             checkSpeciallog(temp);
             saveLog(temp);
         }

         if(coreFlag)
         {
             sleep(1);
         }
    }
    return 0;
}




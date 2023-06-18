#include "./config/config.h"
//#include "./webserverhttp/webserverhttp.h"
#include "./apptcpsocket/apptcpsocket.h"

#include <iostream>
#include <thread>
using namespace std;

// int main(int argc, char *argv[])
// {
//     //需要修改的数据库信息,登录名,密码,库名
//     string user = "root";
//     string passwd = "root";
//     string databasename = "qgydb";
//     Config config;
//     config.parse_arg(argc, argv);

//     TinyWebServer server;
//     server.init(config.PORT, user, passwd, databasename, config.LOGWrite, 
//                 config.OPT_LINGER,  config.sql_num,  config.thread_num, 
//                 config.close_log, config.actor_model);

//     server.eventListen();
  
//     server.eventloop();

//     return 0;
// }

void httpCommThread() {
    //需要修改的数据库信息,登录名,密码,库名
    string user = "root";
    string passwd = "lab101zf!";
    string databasename = "mysql";
    Config config;
    //config.parse_arg(argc, argv);

    TinyWebServer server;
    server.init(config.PORT, user, passwd, databasename, config.LOGWrite, 
                config.OPT_LINGER,  config.sql_num,  config.thread_num, 
                config.close_log, config.actor_model);
    //数据库
   

    server.eventListen();

    server.sql_pool();
    
    server.eventloop();
    


}

// TCP通信线程函数
void tcpCommThread() {
     APPTCP apptcpspec;
    apptcpspec.socketinit(10010);
    apptcpspec.dataReceiverThread();
}

int main(int argc, char *argv[]) {


    // 创建HTTP通信线程
    std::thread httpThread(httpCommThread);

    // 创建TCP通信线程
    std::thread tcpThread(tcpCommThread);

    // 等待线程结束
    httpThread.join();
    tcpThread.join();

    return 0;
}








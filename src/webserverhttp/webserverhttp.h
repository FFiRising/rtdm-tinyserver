#ifndef TINYWEBSERVER_H
#define TINYTWEBSERVER_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>

#include <signal.h>
#include "assert.h"
#include <string>

#include "../lock/locker.h"
#include "../threadpool/threadpool.h"
#include "../http_connection/http_conn.h"
#include "../timer/timer.h"

using namespace std;
#define MAX_FD 65535  //最大文件描述符个数
#define MAX_EVENT_NUMBER 10000
#define MAX_timer_clients 65536




class TinyWebServer{

public:
    TinyWebServer();
    ~TinyWebServer();
    void init(int port , string user, string passWord, string databaseName,
              int log_write , int opt_linger,  int sql_num,
              int thread_num, int close_log, int actor_model);
    void eventListen();
    void eventloop();
    void sql_pool();
   // bool dealwithsignal(bool &timeout);
   // void sig_handler(int sig);

public:

    //连接属性
    int  m_port; 
    char *m_root; //文件路径字符串
    int m_log_write; //日志
    int m_close_log;    
    int m_actormodel;   //模式
    int m_OPT_LINGER;   //是否优雅关闭连接 

    int m_epollfd;

    //定时堆
    bool timeout;
    int TIMESLOT;



    //数据库相关
    connection_pool *m_connPool;

    string m_user;         //登陆数据库用户名
    string m_passWord;     //登陆数据库密码
    string m_databaseName; //使用数据库名
    int m_sql_num;

    //线程池
    threadpool<http_conn> *pool;
    int m_thread_num;
    
    //连接实例
    std::list<std::weak_ptr<http_conn>> close_list;
    std::vector<std::shared_ptr<http_conn>> users;

    //epoll
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd;

    //listen
     int m_listenfd;
    
};




#endif
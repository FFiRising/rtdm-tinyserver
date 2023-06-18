#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <sys/epoll.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include "../lock/locker.h"
#include <sys/uio.h>
#include <string.h>
#include <memory>
#include <map>
#include <list>

#include "../sharedbuffer/sharedbuffer.h"
#include "../CGImysql/sql_connection_pool.h"

class  http_conn{

public:
   

    //http请求方法
    enum METHOD{
        GET = 0, 
        POST, 
        HEAD, 
        PUT, 
        DELETE, 
        TRACE, 
        OPTIONS, 
        CONNECT
    };

    /*解析客户端请求时，主状态机状态
    CHECK_STATE_REQUESTLINE :当前正在分析请求行
    CHECK_STATE_HEADER:当前正在分析头部字段
    CHECK_STATE_CONTENT:当前正在解析请求体
    */
   enum CHECK_STATE{
    CHECK_STATE_REQUESTLINE = 0,
    CHECK_STATE_HEADER ,
    CHECK_STATE_CONTENT 
   };
    
    //从状态机的三种可能可能状态， 即行的读取状态，分别表示
    //1. 读取到一个完整的行  2.行出错 3.行数据尚且不完整
    enum LINE_STATUS{
        LINE_OK = 0,
        LINE_BAD   ,
        LINE_OPEN    
    };



   /*服务器处理http请求的可能结果，报文解析结果
        NO_REQUEST          : 请求不完整， 需要继续读取客户端数据
        GET_REQUEST         : 表示获得了一个完整的客户请求
        BAD_REQUEST         : 表示客户请求语法错误
        NO_REQUEST          : 表示服务器没有资源
        FORBIDDEN_REQUESE   :   表示客户对资源无足够权限
        FILE_REQUEST        :   文件请求，获取文件成功
        INTERNAL_ERROR      :   表示服务器内部错误
        CLOSED_CONNECTION   :   表示客户端已经关闭连接
   */
  enum HTTP_CODE{
        NO_REQUEST          ,
        GET_REQUEST         ,
        BAD_REQUEST         ,
        NO_RESOURCE         ,
        FORBIDDEN_REQUEST   ,
        FILE_REQUEST        ,
        INTERNAL_ERROR      ,
        CLOSED_CONNECTION   ,
        FILE_SPEC           ,
        FILE_JSON
    };

   enum IO_STATUS{
        HAVE_DATA_TO_READ = 0,
        HAVE_DATA_TO_WRITE,
        HAVE_NOTHING
    };
    enum STATUS{
        REACTOR = 0,
        PROACTOR
    };
public:
    static int m_epollfd; //所有socket上的事件都被注册到同意epollfd上
    static int m_user_count;//统计用户数量
    static const int READ_BUFFER_SIZE = 2048;//读缓冲区大小
    static const int WRITE_BUFFER_SIZE = 1024;//写缓冲区大小
    static const int FILENAME_LEN = 200;      // 文件名的最大长度
    static int m_users_count;
    static STATUS m_status;

public:

    http_conn(){};
    ~ http_conn(){};

public:
    void process(); // 处理客户端请求 解析请求 拼接资源 发送数据 
   // void init(int sockfd, const sockaddr_in &addr, connection_pool *connPool); //初始化新接收的连接
    void init(int sockfd, const sockaddr_in &addr); //初始化新接收的连接
    void close_conn(); //关闭连接
    bool read(); //非阻塞
    bool write(); //非阻塞
    bool setIOState(int state);
    void execute();

public:
    void initmysql_result(connection_pool *connPool);

private:
    int m_sockfd;//该http连接的socket
    sockaddr_in m_address;//通信的socket地址

    CHECK_STATE m_check_state; //主状态机当前所在状态

    char m_read_buf[READ_BUFFER_SIZE];//读缓冲区
    int m_read_idx;         //标识读缓冲区中以及读入的客户数据的最后一个字节的下个位置
    int m_checked_index;    // 当前正在分析的字符在读缓冲区位置
    int m_start_line;       //当前正在解析的起始位置

    char m_real_file [ FILENAME_LEN ];       // 客户请求的目标文件的完整路径，其内容等于 doc_root + m_url, doc_root是网站根目录
    char *  m_url;          //请求目标的文件名
    char* m_verson;         // 协议版本 
    METHOD m_method;         //请求方法
    char* m_host;           //主机名
    bool m_linger;          //http请求是否保持连接
    int m_content_length; 

    char m_write_buf[ WRITE_BUFFER_SIZE ];  // 写缓冲区
    int m_write_idx;                        // 写缓冲区中待发送的字节数
    char* m_file_address;                   // 客户请求的目标文件被mmap到内存中的起始位置
    struct stat m_file_stat;                // 目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
    struct iovec m_iv[2];                   // 我们将采用writev来执行写操作，所以定义下面两个成员，其中m_iv_count表示被写内存块的数量。
    int m_iv_count;

    int bytes_to_send;              // 将要发送的数据的字节数
    int bytes_have_send;            // 已经发送的字节数
    int m_TRIGEMODE; 
    IO_STATUS m_io_state;
    HTTP_CODE m_read_ret;
    int m_client_sock;

    int m_post_cgi ; 
    char *m_string; //存储请求头数据
    MYSQL *mysql;
   // connection_pool *m_connPool;

private:
    void init(); //初始化连接其他信息
    HTTP_CODE process_read(); // 解析http请求
    HTTP_CODE parse_rquest_line(char * text);//解析请求首行
    HTTP_CODE parse_headers(char *text);//解析请求头
    HTTP_CODE parse_content(char *text);//解析请求体
    LINE_STATUS parse_line();
    char * getline(){return m_read_buf + m_start_line;}
    HTTP_CODE do_request();

    bool process_write( HTTP_CODE ret );    // 填充HTTP应答
    // 这一组函数被process_write调用以填充HTTP应答。
    void unmap();
    bool add_response( const char* format, ... );
    bool add_content( const char* content );
    bool add_content_type();
    bool add_status_line( int status, const char* title );
    bool add_headers( int content_length );
    bool add_content_length( int content_length );
    bool add_linger();
    bool add_blank_line();

};




#endif
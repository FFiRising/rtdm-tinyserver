#include "http_conn.h"

static rtdm::Logger::ptr http_logger = RTDM_LOG_NAME("system");

//处理mysql连接
locker m_lock;
map<string, string> users;


int http_conn::m_epollfd = 1 ; //所有socket上的事件都被注册到同一epollfd上
int http_conn::m_user_count = 0;//统计用户数量
http_conn::STATUS http_conn::m_status = http_conn::REACTOR;

// 定义HTTP响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

// 网站的根目录
const char* doc_root = "/home/codez/LinuxTest/rtdmServer/resources";



//设置文件描述符非阻塞
int   setnonblocking(int fd){
    int old_flag = fcntl(fd, F_GETFL);
    int new_flag= old_flag | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flag);
    return old_flag;
}

// 向epoll中添加需要监听的文件描述符
void addfd(int epollfd, int fd, bool one_shot, int TRIGEMODE){
    epoll_event event;
    event.data.fd = fd;
    if(TRIGEMODE == 1)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP; //对端断开不用
    if(one_shot){
        // 防止同一个通信被不同的线程处理
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    //设置文件描述符非阻塞
    setnonblocking(fd);

}

//delete fd from epoll
void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//modify file descriptor ; 
//reset oneshot int socket to  make sure EPOLLIN be trigger when data could be read   
void modfd(int epollfd, int fd, int ev){
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP | EPOLLET ;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}



//初始化连接
void http_conn::init(int sockfd, const sockaddr_in &addr){
    m_sockfd = sockfd;
    m_address = addr;
    // //设置socketfd 的端口复用
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    //添加到epoll 对象中
    addfd(m_epollfd, m_sockfd, true, 1);
    m_user_count ++; //总用户数加一
    
    init();
    //RTDM_LOG_FMT_INFO(g_logger, "client : %s init ", to_string(m_sockfd));
    //printf("客户端%d 初始化\n", m_sockfd);
    RTDM_LOG_FMT_INFO(http_logger, "info: client %d init\n", sockfd);
}

//初始化属性
void http_conn::init(){
    
   
    bytes_to_send = 0;
    bytes_have_send = 0;

    m_check_state = CHECK_STATE_REQUESTLINE; //初始化状态为解析首行
    m_checked_index = 0;
    m_start_line = 0;
    m_read_idx = 0;
    m_method = GET;
    m_url = 0;
    m_verson = 0;
    m_linger = false;
    m_io_state = HAVE_NOTHING;

    m_content_length = 0;
    m_host = 0;
    m_write_idx = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);//bezero出现段错误 排查原因
    memset(m_real_file, '\0',FILENAME_LEN);
    // bzero(m_read_buf, READ_BUFFER_SIZE); 
    // bzero(m_write_buf, READ_BUFFER_SIZE);
    // bzero(m_real_file, FILENAME_LEN);

    mysql = mysql_init(nullptr);

}


//关闭连接
void http_conn ::close_conn(){
    if(m_sockfd != -1){
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count --;
    }
}

//读
bool http_conn ::read(){
    if(m_read_idx >= READ_BUFFER_SIZE){
        return false;
    }
    int bytes_read = 0;
    while (true)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if(bytes_read == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                break;//无数据
            }
            return false;
        }
        else if(bytes_read == 0){
            //对方关闭连接
            return false;
        }
        m_read_idx += bytes_read;
    }
    //printf("读取到数据： %s\n", m_read_buf);
    return true;
}

// 主状态机 解析请求
http_conn::HTTP_CODE http_conn::process_read(){

    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    //printf("m_check_state is  %d\n parse_line() is %d\n ", m_check_state, parse_line());

    while( ( (m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK) ) || ((line_status = parse_line()) == LINE_OK ) ){
            //解析到了一行完整数据 或者解析到了请求体完整数据 
            //获取一行数据
            text = getline();
            m_start_line = m_checked_index;
            //printf("got 1 http line: %s\n", text);
            switch (m_check_state)
            {
                case CHECK_STATE_REQUESTLINE:
                {
                    ret =parse_rquest_line(text); //请求行
                  
                    if(ret == BAD_REQUEST){
                        return BAD_REQUEST;
                    }
                    break;
                }
                
                case  CHECK_STATE_HEADER :
                {
                    ret = parse_headers(text);
                    if(ret == BAD_REQUEST){
                        return BAD_REQUEST;
                    }
                    else if(ret == GET_REQUEST){
                        return do_request();
                    }
                    break;
                }
                case CHECK_STATE_CONTENT:
                {
                    ret = parse_content(text);
                    if(ret == GET_REQUEST){
                        return do_request();
                    }
                    line_status = LINE_OPEN;
                    break;
                }
                default:
                {
                    return INTERNAL_ERROR;
                    break;
                }
            }
        }
    return NO_REQUEST;
}

//解析http请求行 获得请求方法 目标URL http版本
http_conn::HTTP_CODE http_conn::parse_rquest_line(char * text){

    // GET /index.html HTTP/1.1
    m_url = strpbrk(text, " \t"); // 判断第二个参数中的字符哪个在text中最先出现 一定注意是空格+ /t debug很长时间
    if (! m_url) { 
        return BAD_REQUEST;
    }
    // GET\0/index.html HTTP/1.1
    *m_url++ = '\0';    // 置位空字符，字符串结束符
    char* method = text;
    if ( strcasecmp(method, "GET") == 0 ) { // 忽略大小写比较
        m_method = GET;
    } 
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        m_post_cgi = 1;
    }
    else {
        return BAD_REQUEST;
    }
    // /index.html HTTP/1.1
    // 检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标。
    m_verson = strpbrk( m_url, " \t" );
    if (!m_verson) {
        return BAD_REQUEST;
    }
    *m_verson++ = '\0';
    if (strcasecmp( m_verson, "HTTP/1.1") != 0 ) {
        return BAD_REQUEST;
    }
    /**
     * http://192.168.110.129:10000/index.html
    */
    if (strncasecmp(m_url, "http://", 7) == 0 ) {   
        m_url += 7;
        // 在参数 str 所指向的字符串中搜索第一次出现字符 c（一个无符号字符）的位置。
        m_url = strchr( m_url, '/' );
    }
    if ( !m_url || m_url[0] != '/' ) {
        return BAD_REQUEST;
    }
  
    m_check_state = CHECK_STATE_HEADER; // 检查状态变成检查头
    //printf("  m_method: %d\n  m_verson: %s\n  m_url :%s\n  m_check_state %d\n" , m_method, m_verson, m_url, m_check_state);
    RTDM_LOG_FMT_INFO(http_logger, " m_method: %d\n  m_verson: %s\n  m_url :%s\n  m_check_state %d\n" , m_method, m_verson, m_url, m_check_state);
    return NO_REQUEST;

}


http_conn::HTTP_CODE http_conn::parse_headers(char *text){

    // 遇到空行，表示头部字段解析完毕
    if( text[0] == '\0' ) {
        // 如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体，
        // 状态机转移到CHECK_STATE_CONTENT状态
        if ( m_content_length != 0 ) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        // 否则说明我们已经得到了一个完整的HTTP请求
        return GET_REQUEST;
    } 

    else if ( strncasecmp( text, "Connection:", 11 ) == 0 ) {
        // 处理Connection 头部字段  Connection: keep-alive
        text += 11;
        text += strspn( text, " \t" );
        if ( strcasecmp( text, "keep-alive" ) == 0 ) {
            m_linger = true;
        }
    } 
    else if ( strncasecmp( text, "Content-Length:", 15 ) == 0 ) {
        // 处理Content-Length头部字段
        text += 15;
        text += strspn( text, " \t" );
        m_content_length = atol(text);
    } 
    else if ( strncasecmp( text, "Host:", 5 ) == 0 ) {
        // 处理Host头部字段
        text += 5;
        text += strspn( text, " \t" );
        m_host = text;
    } 
    else {
        //printf( "oop! unknow header %s\n", text );
        RTDM_LOG_FMT_ERROR(http_logger, "oop! unknow header %s\n", text);
    }
    return NO_REQUEST;
}


http_conn::HTTP_CODE http_conn::parse_content(char *text){

    if ( m_read_idx >= ( m_content_length + m_checked_index ) )
    {
        text[ m_content_length ] = '\0';
        m_string = text; //post请求text字段为用户名和密码
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

//解析一行 判断 \r\n
http_conn::LINE_STATUS http_conn::parse_line(){
    
    char temp;
    for( ; m_checked_index < m_read_idx; ++m_checked_index){
        temp = m_read_buf[m_checked_index];
        if(temp == '\r'){
            if((m_checked_index + 1) == m_read_idx){
                return LINE_OPEN;
            }
            else if(m_read_buf[m_checked_index + 1] == '\n'){
                m_read_buf[m_checked_index ++] = '\0';
                m_read_buf[m_checked_index ++ ]= '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    
        else if(temp == '\n'){
            if((m_checked_index > 1) && (m_read_buf[m_checked_index - 1] == '\r')){
                m_read_buf[m_checked_index - 1] = '\0';
                m_read_buf[m_checked_index ++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }  
    return LINE_OPEN; //不完整
}


// 当得到一个完整、正确的HTTP请求时，我们就分析目标文件的属性，
// 如果目标文件存在、对所有用户可读，且不是目录，则使用mmap将其
// 映射到内存地址m_file_address处，并告诉调用者获取文件成功
// 包括对不同url的处理
http_conn::HTTP_CODE http_conn::do_request(){

// /home/codez/LinuxTest/TinyWebServer2.0/resources
    strcpy( m_real_file, doc_root );
    int len = strlen( doc_root );

    const char *p = strrchr(m_url, '/');



    //处理cgi m_post_cgi==1 为post方法 处理上来的content首位， 3为注册，2为登录
    if (m_post_cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {
        
        //根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        //cgi to file name
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //将用户名和密码提取出来
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 9; m_string[i] != '&'; ++i)
            name[i - 9] = m_string[i];
        name[i - 9] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';
        //cout << "post test 1111" <<endl;

        if (*(p + 1) == '3')
        {
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO labuser(labusername, labpasswd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            //cout << "post test 2222" <<endl;
           
            if (users.find(name) == users.end())
            {
                
                m_lock.lock();


                int res = mysql_query(mysql, sql_insert);

                users.insert(pair<string, string>(name, password));
                m_lock.unlock();

                if (!res)
                    strcpy(m_url, "/logandreg.html"); 
                    RTDM_LOG_FMT_INFO(http_logger, "用户数据插入 INSERT INTO labuser(labusername : %s, labpasswd : %s)" , name, password);

                else
                    strcpy(m_url, "/logerror.html");
            }
            else
                strcpy(m_url, "/logerror.html");
        }
        
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password)
                //strcpy(m_url, "/TP2.0.html");
            {
                RTDM_LOG_FMT_INFO(http_logger, "用户登陆成功 (labusername : %s, labpasswd : %s)" , name, password);
                strcpy(m_url, "/TP2.0.html");
                //strcpy(m_url, "/index.html");
            }
            else
            {
                 RTDM_LOG_FMT_INFO(http_logger, "用户登陆失败 (labusername : %s, labpasswd : %s)" , name, password);
                strcpy(m_url, "/logerror.html");
            }
        }
    }

    // if (*(p + 1) == '0')
    // {
    //     char *m_url_real = (char *)malloc(sizeof(char) * 200);
    //     strcpy(m_url_real, "/register.html");
    //     strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    //     free(m_url_real);
    // }
    // else if (*(p + 1) == '1')
    // {
    //     char *m_url_real = (char *)malloc(sizeof(char) * 200);
    //     strcpy(m_url_real, "/log.html");
    //     strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    //     free(m_url_real);
    // }
    // else if (*(p + 1) == '5')
    // {
    //     char *m_url_real = (char *)malloc(sizeof(char) * 200);
    //     strcpy(m_url_real, "/picture.html");
    //     strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    //     free(m_url_real);
    // }
    // else if (*(p + 1) == '6')
    // {
    //     char *m_url_real = (char *)malloc(sizeof(char) * 200);
    //     strcpy(m_url_real, "/video.html");
    //     strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    //     free(m_url_real);
    // }
    // else if (*(p + 1) == '7')
    // {
    //     char *m_url_real = (char *)malloc(sizeof(char) * 200);
    //     strcpy(m_url_real, "/fans.html");
    //     strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    //     free(m_url_real);
    // }





    //处理get json / specdata的请求
    if( strcmp(m_url, "/specdata") == 0){
        return FILE_SPEC;
            RTDM_LOG_INFO(http_logger)<< "用户获取json spec 数据 ";
            
    }
    if( strcmp(m_url, "/jsondata") == 0){
            RTDM_LOG_INFO(http_logger) <<"用户获取jsondata 数据 ";
        return FILE_JSON;

    }


    strncpy( m_real_file + len, m_url, FILENAME_LEN - len - 1 );

    // 获取m_real_file文件的相关的状态信息，-1失败，0成功
    if ( stat( m_real_file, &m_file_stat ) < 0 ) {
        return NO_RESOURCE;
    }

    // 判断访问权限
    if ( ! ( m_file_stat.st_mode & S_IROTH ) ) {
        return FORBIDDEN_REQUEST;
    }

    // 判断是否是目录
    if ( S_ISDIR( m_file_stat.st_mode ) ) {
        return BAD_REQUEST;
    }

    // 以只读方式打开文件
    int fd = open( m_real_file, O_RDONLY );

    // 创建内存映射
    m_file_address = ( char* )mmap( 0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
    close( fd );
    return FILE_REQUEST;
 }

// 对内存映射区执行munmap操作
void http_conn::unmap() {
    if( m_file_address )
    {
        munmap( m_file_address, m_file_stat.st_size );
        m_file_address = 0;
    }
}


    


// 往写缓冲中写入待发送的数据  可变参数 被调用
bool http_conn::add_response( const char* format, ... ) {
    if( m_write_idx >= WRITE_BUFFER_SIZE ) {
        return false;
    }
    va_list arg_list;
    va_start( arg_list, format );
    int len = vsnprintf( m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list );
    if( len >= ( WRITE_BUFFER_SIZE - 1 - m_write_idx ) ) {
        return false;
    }
    m_write_idx += len;
    va_end( arg_list );
    return true;
}

//添加响应行状态
bool http_conn::add_status_line( int status, const char* title ) {
    return add_response( "%s %d %s\r\n", "HTTP/1.1", status, title );
}

//添加响应头状态
bool http_conn::add_headers(int content_len) {
    add_content_length(content_len);
    add_content_type();
    add_linger();
    add_blank_line();
}
//响应 内容长度
bool http_conn::add_content_length(int content_len) {
    return add_response( "Content-Length: %d\r\n", content_len );
}

//响应 内容类型
bool http_conn::add_content_type() {
    if(m_read_ret == FILE_JSON) {
         return add_response("Content-Type:%s\r\n", "application/json");
    }
    return add_response("Content-Type:%s\r\n", "text/html;charset=UTF-8");
}

//响应 是否alive
bool http_conn::add_linger()
{
    return add_response( "Connection: %s\r\n", ( m_linger == true ) ? "keep-alive" : "close" );
}

//响应 空行
bool http_conn::add_blank_line()
{
    return add_response( "%s", "\r\n" );
}

//响应 字符内容
bool http_conn::add_content( const char* content )
{
    return add_response( "%s", content );
}




// 根据服务器处理HTTP请求的结果，决定返回给客户端的内容 若此函数返回真，可往客户端写入
bool http_conn::process_write(HTTP_CODE ret) {
    switch (ret)
    {
        case INTERNAL_ERROR:
            add_status_line( 500, error_500_title );
            add_headers( strlen( error_500_form ) );
            if ( ! add_content( error_500_form ) ) {
                return false;
            }
            break;
        case BAD_REQUEST:
            add_status_line( 400, error_400_title );
            add_headers( strlen( error_400_form ) );
            if ( ! add_content( error_400_form ) ) {
                return false;
            }
            break;
        case NO_RESOURCE:
            add_status_line( 404, error_404_title );
            add_headers( strlen( error_404_form ) );
            if ( ! add_content( error_404_form ) ) {
                return false;
            }
            break;
        case FORBIDDEN_REQUEST:
            add_status_line( 403, error_403_title );
            add_headers(strlen( error_403_form));
            if ( ! add_content( error_403_form ) ) {
                return false;
            }
            break;
        case FILE_REQUEST:
            if(m_method == GET){
                add_status_line(200, ok_200_title );
                add_headers(m_file_stat.st_size);
                m_iv[ 0 ].iov_base = m_write_buf;
                m_iv[ 0 ].iov_len = m_write_idx;
                m_iv[ 1 ].iov_base = m_file_address;
                m_iv[ 1 ].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                bytes_to_send = m_write_idx + m_file_stat.st_size;
                return true;
            }
            else if(m_method == POST){
                cout << "test in post ,process wirte" << endl;
                add_status_line(200, ok_200_title );
                add_headers(m_file_stat.st_size);
                m_iv[ 0 ].iov_base = m_write_buf;
                m_iv[ 0 ].iov_len = m_write_idx;
                m_iv[ 1 ].iov_base = m_file_address;
                m_iv[ 1 ].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                bytes_to_send = m_write_idx + m_file_stat.st_size;
                return true;

            }

        case FILE_SPEC:
          
            RTDM_LOG_INFO(http_logger)<< "服务器写json spec 数据 ";
            if(m_method == GET){
                add_status_line(200, ok_200_title );
                add_headers((uint8_t)sizeof(spec_share_buf));
                m_iv[ 0 ].iov_base = m_write_buf;
                m_iv[ 0 ].iov_len = m_write_idx;
                m_iv[ 1 ].iov_base = spec_share_buf;
                m_iv[ 1 ].iov_len = (uint8_t)sizeof(spec_share_buf);
                m_iv_count = 2;
                bytes_to_send = m_write_idx +(uint8_t)sizeof(spec_share_buf);
                return true;
            }
        
        case FILE_JSON:{
            RTDM_LOG_INFO(http_logger)<< "服务器写jsondata 数据 " ;
            if(m_method == GET){
                add_status_line(200, ok_200_title );
                add_headers(Json_share_buf.length());


                //传递char* 数据
                char * json_char_shared = (char *)malloc(256);
                memset(json_char_shared,'\0',256);
                memcpy(json_char_shared, Json_share_buf.data(), Json_share_buf.length());

               // void * tempjsonshared = json_char_shared;
                m_iv[ 0 ].iov_base = m_write_buf;
                m_iv[ 0 ].iov_len = m_write_idx;
                m_iv[ 1 ].iov_base = json_char_shared;
                m_iv[ 1 ].iov_len = Json_share_buf.length();
                m_iv_count = 2;
                bytes_to_send = m_write_idx +Json_share_buf.length();
                return true;
            }
        }
        default:
            return false;
    }

    m_iv[ 0 ].iov_base = m_write_buf;
    m_iv[ 0 ].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

bool http_conn::write(){
    
    int temp = 0;
    if ( bytes_to_send == 0 ) {
        // 将要发送的字节为0，这一次响应结束。
        modfd( m_epollfd, m_sockfd, EPOLLIN ); 
        init();
        return true;
    }
        RTDM_LOG_FMT_INFO(http_logger, "bytes_to_send %d\n", bytes_to_send);
       // printf("bytes_to_send %d\n", bytes_to_send);
    while(1) {
        // 集中写
        temp = writev(m_sockfd, m_iv, m_iv_count);
        //printf("temppppppp %d\n", temp);

        if ( temp <= -1 ) {
            // 如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，虽然在此期间，
            // 服务器无法立即接收到同一客户的下一个请求，但可以保证连接的完整性。
            if( errno == EAGAIN ) {
                if(m_io_state == HAVE_DATA_TO_WRITE && bytes_to_send != 0){
                    // modfd( m_epollfd, m_sockfd, EPOLLOUT);
                    // return true;
                    continue;
                }
                
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;

        //printf("bytes_have_send %d\n", bytes_have_send);
        //printf("bytes_to_send %d\n", bytes_to_send);

        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - temp;
        }

        if (bytes_to_send <= 0)
        {
            // 没有数据要发送了
            
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN);
            
            if (m_linger)
            {
                
                init();
                RTDM_LOG_INFO(http_logger)<< "数据发送完毕" ;
                //printf("数据已发送完毕\n");
                return true;
              
            }
            else
            {  
                return false;
            }
        }

    }

}


//有线程池中的工作线程调用，处理http请求的的入口函数 判断进入读状态机还是写状态机
void http_conn::process(){
    
    //解析http
    //cout << "read_ret "<<  endl;
    HTTP_CODE read_ret = process_read();
    

    if(read_ret == NO_REQUEST){
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return ;
    }

    // 生成响应
   bool write_ret = process_write( read_ret );
    if ( !write_ret ) {
        close_conn();
    }
    modfd( m_epollfd, m_sockfd, EPOLLOUT);

}

bool http_conn::setIOState(int state){
    switch(state){
        case 0:{
            m_io_state = HAVE_DATA_TO_READ;
            return true;
        }
        case 1:{
            m_io_state = HAVE_DATA_TO_WRITE;
            return true;
        }
        case 2:{
            m_io_state = HAVE_NOTHING;
            return false;
        }
        default:
            break;
    }
    return false;
}

void http_conn::execute(){
    switch (m_status)
    {
    case REACTOR:{
        if(m_io_state == HAVE_DATA_TO_READ && read()){
            m_read_ret = process_read();
          
            if(m_read_ret == NO_REQUEST){
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return ;
            }
            if(m_read_ret == FILE_REQUEST)
                modfd(m_epollfd, m_sockfd, EPOLLOUT); 

            if(m_read_ret == FILE_SPEC)  
                modfd(m_epollfd, m_sockfd, EPOLLOUT); 

            if(m_read_ret == FILE_JSON)  
                modfd(m_epollfd, m_sockfd, EPOLLOUT); 
               
        }
        else if(m_io_state == HAVE_DATA_TO_WRITE && process_write(m_read_ret)){
            write();
        }
        break;
    }
    case PROACTOR:{
        m_read_ret = process_read();
        process_write(m_read_ret);
        modfd(m_epollfd, m_sockfd, EPOLLOUT);
    }
    default:
        break;
    }
}

void http_conn::initmysql_result(connection_pool *connPool)
{
    //先从连接池中取一个连接
    //MYSQL *mysql = NULL;
    //MYSQL* mysql = mysql_init(nullptr);
    connectionRAII mysqlcon(&mysql, connPool);

    //在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT labusername, labpasswd FROM labuser"))
    {
       // LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
       //printf (" query error") ;
       RTDM_LOG_FMT_ERROR(http_logger, "SELECT error:%s\n", "mysql_error");
    }
    
    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);
  
    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);
    
    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}

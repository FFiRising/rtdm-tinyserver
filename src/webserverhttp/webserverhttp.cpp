#include "webserverhttp.h"
#include "../log/log.h"

static rtdm::Logger::ptr ws_logger = RTDM_LOG_NAME("system");
static int pipefd[2];
static time_heap heap; 
static client_data * users_data;

 
//add signal capture
void addsig(int sig, void(handler)(int), bool restart = true){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));//置零
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);//临时阻塞信号集
    assert( sigaction( sig, &sa, NULL ) != -1 );
}

//add file descriptor
extern void addfd(int epollfd, int fd, bool one_short, int TRIGEMODE);

//delete fd from epoll
extern void removefd(int epollfd, int fd);

//modify file descriptor
extern void modf(int epollfd, int fd, int ev);

extern int  setnonblocking(int fd);

//定时器回调函数，它删除非活动连接socket上的注册事件，并关闭之
void cb_func(int epollfd, client_data* user_data)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}

// 定时处理任务，调用tick()函数
void timer_handler(time_heap& heap, int TIMESLOT)
{
    //
   
    heap.tick();
    // 因为一次 alarm 调用只会引起一次SIGALARM 信号，所以我们要重新定时，以不断触发 SIGALARM信号。
    alarm(TIMESLOT);
}

//sigalarm触发信号队列
void sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    
	send(pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

bool dealwithsignal(bool &timeout)
{
    int ret = 0;
    int sig;
    char signals[1024];
    //memset(signals, '\0', 1024);
    ret = recv(pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1)
    {
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }
    else
    {   
        for (int i = 0; i < ret; ++i)
        {
            switch (signals[i])
            {
            case SIGALRM:
            {
                timeout = true;// 用timeout变量标记有定时任务需要处理，但不立即处理定时任务
                                // 这是因为定时任务的优先级不是很高，我们优先处理其他更重要的任务
				//printf("timeout\n");
                RTDM_LOG_INFO(ws_logger)<< "Time out ";
                break;
            }
            case SIGTERM:
            {
                break;
            }
            }
        }
    }
    
    return true;
}


TinyWebServer::TinyWebServer(){

    //创建线程池 初始化
          
	users_data = new client_data[MAX_timer_clients];   
    http_conn::m_user_count = 0;
  
    //用户任务对象初始化
   // std::vector<std::shared_ptr<http_conn>> users(10000);  
     
}


TinyWebServer::~TinyWebServer(){

    close(epollfd);
    close(m_listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete pool;

}

void TinyWebServer::init(int port, string user, string passWord, string databaseName, int log_write, 
                     int opt_linger,  int sql_num, int thread_num, int close_log, int actor_model){
    m_port = port;

    m_user = user;
    m_passWord = passWord;

    m_databaseName = databaseName;
    m_sql_num = sql_num;

    m_thread_num = thread_num;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_close_log = close_log;
    m_actormodel = actor_model;

    timeout = false;


}


void TinyWebServer::eventListen(){

 
   

    addsig(SIGPIPE, SIG_IGN);               //对SIGPIE信号进行处理
    addsig(SIGALRM, sig_handler, false);    //在定时器设置的超时时间到达后，调用alarm的进程将收到SIGALRM信号。
	addsig(SIGTERM, sig_handler, false);    //软件发出终止信号则调用处理函数                    


    // //优雅关闭连接
    // if (0 == m_OPT_LINGER)
    // {
    //     struct linger tmp = {0, 1};
    //     setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    // }
    // else if (1 == m_OPT_LINGER)
    // {
    //     struct linger tmp = {1, 1};
    //     setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    // }

     //pair初始化 
    int ret = 0;
	ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    setnonblocking(pipefd[1]);

    //listen
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    // 端口复用
    int reuse = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    //绑定listen socket
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = PF_INET;
    address.sin_port = htons(m_port);
    address.sin_addr.s_addr = INADDR_ANY;
    bind(m_listenfd, (struct  sockaddr *)&address, sizeof(address));

    //listen
    listen(m_listenfd, 5);

    epollfd = epoll_create(5);
    //添加到epoll 
    addfd(epollfd, m_listenfd, false, 1); //ET + NOBLOCKING
    addfd(epollfd, pipefd[0], false, 1);  // ET + NOBLOCKING
    http_conn::m_epollfd = epollfd;
    heap.set_epollfd(epollfd);

    //启动定时
     TIMESLOT = 20;
	 bool flag = true;
	 alarm(TIMESLOT);
    
    //http连接 vector
   //std::vector<std::shared_ptr<http_conn>> users(1000);
    users.resize(MAX_FD);
    for(int i = 0; i < MAX_FD; i++){
        users[i] = std::make_shared<http_conn>();
    }
    //线程池
    try{
      pool = new threadpool<http_conn>();
    }catch(...){
         delete pool;
         throw;
    }  

}

void TinyWebServer::eventloop(){
     //主线程
    while(true){
       int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if((num < 0) && (errno != EINTR)){
            //printf("epoll failure\n");
            RTDM_LOG_ERROR(ws_logger)<< "Epoll Failure ";
            break;
        }
  
        //循环遍历数组
        for(int i = 0; i < num; i++){
            int sockfd = events[i].data.fd;
            if(sockfd == m_listenfd){
                //客户端连接
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(client_address);
                int connfd = accept(m_listenfd, (struct sockaddr *) &client_address, &client_addrlen);
                 if ( connfd < 0 ) {
                    //printf( "errno is: %d\n", errno );
                    RTDM_LOG_FMT_ERROR(ws_logger, "Accept Failure ",errno);

                    continue;
                } 
                if(http_conn ::m_user_count >=  MAX_FD){
                    //目前连接数满了，给客户端输出信息： 服务器正忙
                    RTDM_LOG_WARN(ws_logger)<< "服务器正忙\n";
                    close(connfd);
                    continue;
                }

                while(connfd > 0){
					//建立连接并进行初始化
					//printf("connfd is %d\n", connfd);
                 
					addfd(epollfd, connfd, true, 1);  //ET + 非阻塞
                    
				    users[connfd]->init(connfd, client_address);
                    users[connfd]->initmysql_result(m_connPool);
                   
			 	 	users_data[connfd].address = client_address;
				 	users_data[connfd].sockfd = connfd;
                    

					//为http连接设置定时器
				 	heap_timer *timer = new heap_timer(TIMESLOT * 3);
				 	timer->user_data = &users_data[connfd];
				 	timer->cb_func = cb_func;
				 	users_data[connfd].timer = timer;
				 	heap.add_timer(timer);
					connfd = accept(m_listenfd, (struct sockaddr*)&client_address, &client_addrlen);
				    
                }
  

            }
            //timeout
            else if(sockfd == pipefd[0] && events[i].events & EPOLLIN){
				dealwithsignal(timeout);
			 }
            //对方异常断开或者错误等事件
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                heap_timer *timer = users_data[sockfd].timer;
                 if(timer){
					 heap.del_timer(timer);
				 }
                 //shared_ptr未自动释放
				 if(static_cast<int>(users[sockfd].use_count()) != 0){
				 	 users[sockfd]->close_conn();
				 }
				 else{
					close_list.emplace_back(users[sockfd]);
				 }
            }

            else if(events[i].events & EPOLLIN){
                //有数据要读
               
                heap_timer *timer = users_data[sockfd].timer;
				if(users[sockfd]->setIOState(0)){
                   
					pool->append(users[sockfd]);
 
                     //调整生命周期
					if(timer){
						 time_t cur = time(NULL);
						 timer->expire = cur + 3 * TIMESLOT;
						 heap.adjust(timer->m_index);
					}
				 }
                else{
					 if(timer){
					 	heap.del_timer(timer);
				 	 }
					 if(static_cast<int>(users[sockfd].use_count()) == 1){
						cb_func(epollfd, &users_data[sockfd]);
					 }
					 else{
						close_list.emplace_back(users[sockfd]);
				 	}
				 }

            }

            else if(events[i].events & EPOLLOUT){
                //epoll有数据写
                RTDM_LOG_INFO(ws_logger)<< "HAVE DATA TO WRITE \n";
                //printf(" HAVE_DATA_TO_WRITE MAIN   ");
                heap_timer *timer = users_data[sockfd].timer;
				if(users[sockfd]->setIOState(1)){
                    
					 pool->append(users[sockfd]);
					 if(timer){
						 time_t cur = time(NULL);
						 timer->expire = cur + 3 * TIMESLOT;
						 heap.adjust(timer->m_index);
					 }
				 }
				 else{
					 if(timer){
					  	heap.del_timer(timer);
				 	 }	
					 if(static_cast<int>(users[sockfd].use_count()) == 1){
						cb_func(epollfd, &users_data[sockfd]);
					}
					 else{
						 close_list.emplace_back(users[sockfd]);
				 	}
					 
				 }
                RTDM_LOG_INFO(ws_logger)<< "向客户端输出 \n";
                //printf("向客户端输出\n");
            }

        }
        if(timeout){
			 timer_handler(heap, TIMESLOT);
			 timeout = false;
		 }
		 //利用weak_ptr关闭http连接
		 for(std::list<std::weak_ptr<http_conn>>::iterator iter = close_list.begin(); iter != close_list.end();){
			 if(iter->use_count() == 1){
				if(auto p = iter->lock()){
					p->close_conn();
				}
				close_list.erase(iter++);
			 }
			 else{
				 iter++;
			 }
		 }

    }
    close(epollfd);
}

void TinyWebServer::sql_pool()
{
    //初始化数据库连接池
    m_connPool = connection_pool::GetInstance();
    m_connPool->init("localhost", m_user, m_passWord, m_databaseName, 3306, m_sql_num, m_close_log);



    //初始化数据库读取表
    // for(size_t i = 0; i < 2; i++){
    //    users[i] -> initmysql_result(m_connPool); 
    // } 
    RTDM_LOG_INFO(ws_logger)<< "MYSQL INIT ";
   // cout << " initmysql over"<< endl;
    
}
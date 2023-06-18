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
#include "lock/locker.h"
#include "threadpool/threadpool.h"
#include <signal.h>
#include "http_connection/http_conn.h"
#include "assert.h"
#include "./timer/timer.h"

#define MAX_FD 65535 //最大 文件描述符个数
#define MAX_EVENT_NUMBER 10000
#define MAX_timer_clients 65536
int pipefd[2];

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
				printf("timeout\n");
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




int main(int argc, char * argv[]){
    if(argc <= 1){
        printf("按照如下格式运行 : %s port_number \n", basename(argv[0]));
        return 1;
    }

    
    int port = atoi(argv[1]);               //端口号获取
    addsig(SIGPIPE, SIG_IGN);               //对SIGPIE信号进行处理
    addsig(SIGALRM, sig_handler, false);    //在定时器设置的超时时间到达后，调用alarm的进程将收到SIGALRM信号。
	addsig(SIGTERM, sig_handler, false);    //软件发出终止信号则调用处理函数                    
                           
    //创建线程池 初始化
    threadpool<http_conn> *pool = NULL;
    try{
      pool = new threadpool<http_conn>();
    }catch(...){return 1;}
    //定时堆初始化
    static time_heap heap;                  
	client_data* users_data = new client_data[MAX_timer_clients];
	bool timeout = false;
    
    http_conn::m_user_count = 0;
    //http_conn::close_list;
    std::list<std::weak_ptr<http_conn>> close_list;
    //用户任务初始化
    std::vector<std::shared_ptr<http_conn>> users(1000);
    for(int i = 0; i < 1000; i++){
        users[i] = std::make_shared<http_conn>();
    }
	

    //数组 保存客户端任务信息
    // http_conn * users = new http_conn[MAX_FD];

    //pair初始化
    int ret = 0;
	ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    setnonblocking(pipefd[1]);

    //listen
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    // port multiplex before bind
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    //address init bind
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = PF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;
    bind(listenfd, (struct  sockaddr *)&address, sizeof(address));

    //listen
    listen(listenfd, 5);

    //create epoll object
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);

    //添加到epoll 
    addfd(epollfd, listenfd, false, 1);
    addfd(epollfd, pipefd[0], false, 1);
    http_conn::m_epollfd = epollfd;
    heap.set_epollfd(epollfd);

    //启动定时
     int TIMESLOT = 20;
	 bool flag = true;
	 alarm(TIMESLOT);

    //主线程
    while(true){
       int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if((num < 0) && (errno != EINTR)){
            printf("epoll failure\n");
            break;
        }
  
        //循环遍历数组
        for(int i = 0; i < num; i++){
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd){
                //客户端连接
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr *) &client_address, &client_addrlen);
                 if ( connfd < 0 ) {
                    printf( "errno is: %d\n", errno );
                    continue;
                } 
                if(http_conn ::m_user_count >=  MAX_FD){
                    //目前连接数满了，给客户端输出信息： 服务器正忙
                    printf("服务器正忙\n");
                    close(connfd);
                    continue;
                }

                while(connfd > 0){
					//建立连接并进行初始化
					printf("connfd is %d\n", connfd);
					addfd(epollfd, connfd, true, 1);
				    users[connfd]->init(connfd, client_address);
			 	 	users_data[connfd].address = client_address;
				 	users_data[connfd].sockfd = connfd;
					
					//为http连接设置定时器
				 	heap_timer *timer = new heap_timer(TIMESLOT * 3);
				 	timer->user_data = &users_data[connfd];
				 	timer->cb_func = cb_func;
				 	users_data[connfd].timer = timer;
				 	heap.add_timer(timer);
					 connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlen);
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
                printf(" HAVE_DATA_TO_WRITE MAIN   ");
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
                printf("向客户端输出\n");
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
    close(listenfd);
    //delete [] users;
    delete pool;

    return 0;
}
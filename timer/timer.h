#ifndef MIN_HEAP
#define MIN_HEAP


#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>


#include <time.h>
#include <vector>
#include <functional>

#define BUFFER_SIZE 64
class heap_timer;

//用户态结构
struct client_data{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    heap_timer* timer;
};

//定时器类
class heap_timer{
public:
    heap_timer(int delay = 0){
        expire = time(nullptr) + delay;
    }
    time_t expire; //定时器生效时间
    std::function<void(int, client_data*)> cb_func;
    client_data* user_data;
    int m_index;
};

//时间堆类
class time_heap{
public:
    //初始化一个大小为cap的空堆
    time_heap(int cap):capacity(cap), cur_size(0){
        array.reserve(capacity);
        for(int i = 0; i < capacity; i++){
            array.emplace_back(nullptr);
        }
    }
    //无参构造
    time_heap():capacity(0), cur_size(0){
        array.reserve(capacity);
    }
    ~time_heap(){
        array.clear();
    }
    void add_timer(heap_timer* timer);  //添加目标定时器timer
    void del_timer(heap_timer* timer);  //删除目标定时器
    heap_timer* top() const;            //堆顶定时器
    void pop_timer();                   //弹出堆顶定时器
    void tick();                        //心搏函数
    bool empty() const {return cur_size == 0;}          //判断是否为空
    void adjust(int index);                             //调整堆下滤位置                       
    void set_epollfd(int epollfd){m_epollfd = epollfd;} //设置文件描述符

private:
    void percolate_down(int hole);      //最小堆的下滤操作，它确保数组中以第hole个节点为根的子树拥有最小堆性质
    void resize_heap();                 //将堆数组容量扩大一倍

private:
    std::vector<heap_timer*> array;
    int capacity;
    int cur_size; 
    int m_epollfd;
};





#endif


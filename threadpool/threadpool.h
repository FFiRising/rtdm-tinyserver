#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <pthread.h>
#include <list>
#include "../lock/locker.h"
#include <cstdio>
#include <unistd.h>
#include <stdlib.h>
#include <list>
#include <exception>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
 //#include <pthread.h>
//线程池类 模板类 代码复用

class thread{
public:
    pthread_t m_thread;
};


template<typename T>
class threadpool
{

public:
    threadpool(int m_thread_number = 8, int m_max_requests = 10000);
    threadpool(const threadpool&) = delete;
    threadpool& operator= (const threadpool&) = delete;
    ~threadpool();
    //bool append(T* request);
    bool append(std::shared_ptr<T>& request);

private:
    //线程数
    int m_thread_number;

    // //线程池数组
    // pthread_t * m_threads;

    //请求队列最多允许等待处理请求数
    int m_max_requests;

    // //请求队列
    // std::list< T*> m_workqueue;
    
    //互斥锁
    locker m_queuelocker;

    //信号量 判断是否有任务需要处理
    sem m_queuestat;

    //是否结束线程
    bool m_stop;

    std::vector<std::unique_ptr<thread>> m_threads;//描述线程池的数组，其大小为m_thread_number
    std::list<std::shared_ptr<T>> m_workqueue; //请求队列
    std::condition_variable cv;
    std::mutex m_mutex;

private:
    static void * worker(void * arg);
    void run();
};


template <typename T>
threadpool<T>:: threadpool(int thread_number, int max_requests) :
    m_thread_number(thread_number), m_max_requests(max_requests),
    m_stop(false){
    
    //初始判断
    if((thread_number <= 0) || (max_requests <= 0)){
        throw std::exception();
    }
    printf("m_thread_number %d  m_max_requests %d\n", m_thread_number, m_max_requests);
    printf("thread_number%d\n",thread_number);
    /*//申请空间
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads){
        throw std::exception();
    }
    //创建thread_number个线程 并将其设置为线程脱离
    for(int i = 0; i < m_thread_number; ++i){
        printf("create the %dth thread\n", i);
        if(pthread_create(m_threads + i, NULL, worker, this) != 0){
            delete [] m_threads;
            throw std::exception();
        }
        if( pthread_detach(m_threads[i]) ){
            delete[] m_threads;
            throw std::exception();
        }
    } */

    m_threads.reserve(m_thread_number);
    if(m_threads.empty()){
        printf("there is not any thread be created\n");
        //printf(" cap : %ld  size %ld", m_threads.capacity(),  m_threads.size());
    }
    for(int i = 0; i < m_thread_number; i++){
        m_threads.emplace_back(new thread);
        if (pthread_create(&(m_threads[i]->m_thread), NULL, worker, this) != 0){
           
            m_threads.pop_back();
        }
  
        if (pthread_detach(m_threads[i]->m_thread)){
            m_threads.pop_back(); 
        }
        printf("create the %dth thread\n", i);
    }
    
    
}

template <typename T>
threadpool<T>::~threadpool(){
    //delete [] m_threads;
    m_threads.clear();
    m_stop = true;
}

// template <typename T>
// bool threadpool<T> ::append(T * request){
//     m_queuelocker.lock();
//     if(m_workqueue.size() > m_max_requests){
//         m_queuelocker.unlock(); 
//         return false;
//     }
//     m_workqueue.push_back(request);
//     m_queuelocker.unlock();
//     m_queuestat.post();
//     return true;
// }

template <typename T>
bool threadpool<T>::append(std::shared_ptr<T>& request){

    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_workqueue.size() >= m_max_requests) 
        return false;
    m_workqueue.push_back(request);
    cv.notify_one();
    return true;
}

template<typename T>
void * threadpool<T>::worker(void * arg){
    threadpool * pool = (threadpool * ) arg;
    pool->run();
    return pool;
}

// template <typename T>
// void threadpool<T>::run(){
//     while (!m_stop)
//     {
//         m_queuestat.wait();
//         m_queuelocker.lock();
//         if(m_workqueue.empty()){
//             m_queuelocker.unlock();
//             continue;
//         }
//         T* request = m_workqueue.front();
//         m_workqueue.pop_front();
//         m_queuelocker.unlock();
//         if(!request){
//             continue;
//         }
//         request->process();
//     }   
// }

template <typename T>
void threadpool<T>::run()
{
    while (true)
    {
        std::shared_ptr<T> request;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            cv.wait(lock, [&](){return !m_workqueue.empty();});
            request = m_workqueue.front();
            m_workqueue.pop_front();
        }
        if (!request.get()){
            continue;
        }
        request->execute();
    }
}

#endif
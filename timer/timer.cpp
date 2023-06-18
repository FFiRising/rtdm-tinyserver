#include "timer.h"

void time_heap::adjust(int index){
    if(index < capacity){
        percolate_down(index);
    }
}

void time_heap::percolate_down(int hole){
    //最小堆的下虑操作
    heap_timer* temp = array[hole];
    int child = 0;
    for(; (hole * 2 + 1) <= (cur_size - 1); hole = child){
        child = hole * 2 + 1;
        if((child < (cur_size - 1)) && (array[child + 1]->expire < array[child]->expire)){
            //找到子节点中时间最小的节点
            child++;
        }
        if(array[child]->expire < temp->expire){
            array[hole] = array[child];//将最小的节点换到根节点
            array[hole]->m_index = hole;
        }
        else{
            break;
        }
    }
    array[hole] = temp;
    array[hole]->m_index = hole;
}

void time_heap::resize_heap(){
    for(int i = cur_size; i < 2 * capacity; i++){
        array.emplace_back(nullptr);
    }
}

void time_heap::add_timer(heap_timer* timer){
    while(cur_size >= capacity){
        array.emplace_back(nullptr);
        capacity++;
    }
    int hole = cur_size++;//hole为新建空穴的位置
    int parent = 0;
    int count = 0;
    //从空穴到根节点的路径上的所有节点，上滤
    for(; hole > 0; hole = parent){
        parent = (hole - 1) / 2;
        if(array[parent]->expire <= timer->expire){
            break;
        }
        array[hole] = array[parent];
        array[hole]->m_index = hole;
    }
    array[hole] = timer;
    array[hole]->m_index = hole;
}

void time_heap::del_timer(heap_timer* timer){
    /*
        仅将目标定时器的回调函数设置为空，既所谓的延迟销毁，
        这将节省真正删除定时器造成的开销，但这样做容易使得堆数组膨胀
    */
    timer->cb_func = NULL;
    
}

heap_timer* time_heap::top() const{
    if(empty()){
        return nullptr;
    }
    return array[0];
}

void time_heap::pop_timer(){
    if(empty()){
        return;
    }
    if(array[0]){
        array.erase(array.begin());
        array[0] = array[--cur_size];   //将原来的堆顶元素替换为堆数组中最后一个元素
        percolate_down(0);              //堆新的堆顶元素执行下滤操作
    }
}

void time_heap::tick(){
    
    if(array.empty()){
        return ;
    }
    heap_timer* tmp = array[0];
     
    time_t cur = time(nullptr);   
    while(!empty()){
        if(!tmp){
            break;
        }

        //如果堆顶定时器没到期，则退出循环
        if(tmp->expire > cur){
            break;
        }

        //否则就执行堆顶定时器的任务
        if(array[0]->cb_func){
            array[0]->cb_func(m_epollfd, array[0]->user_data);
        }
        pop_timer();
        tmp = array[0];
    }
}

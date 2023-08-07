# rtdm-tinyserver
### 日志模块

​	用于格式化输出程序日志，除了日志内容本身之外，还应该包括文件名/行号，时间戳，线程号，日志级别等额外信息

​	1.日志模块可以通过指定级别实现只输出某个级别以上的日志

​	2.不同的日志可以输出到不同的位置，比如可以输出到标准输出，输出到文件

   3.日志可以分类并命名，一个程序的各个模块可以使用不同的名称来输出日志。

​	4.日志格式可灵活配置。

### 定时器

 通过最小堆定时器高效的定时任务管理，通过时间堆上滤、下滤操作将最小定时维护在堆顶。

### 线程池

  使用一个工作队列完全解除了主线程和工作线程的耦合关系：主线程往工作队列中插入任务，工作线程通过竞争来取得任务并执行它。

### 数据库连接池

- 单例模式，保证唯一
- list实现连接池
- 连接池为静态大小
- 互斥锁实现线程安全

### tcpserver

实现采集卡数据 通过TCP协议进行数据帧封装，服务端解析，json格式进行序列化，将数据压入缓存。

### httpserver

实现 Reactor 模型，利用 EPOLL 的 ET 模式实现多路 IO 复用，可同时处理多个 HTTP 连接。





1、服务器实现了采集卡数据通过 IP 稳定上传, 数据本地缓存。服务器支持静态资源访问和动态消息响应。 

2、用户可使用 URL 进入监控系统页面，网页参量数据每秒刷新一次，系统曾应用于数据采集测试。



感谢：

基于https://github.com/qinguoyi/TinyWebServer.git

基于https://github.com/sylar-yin/sylar.git

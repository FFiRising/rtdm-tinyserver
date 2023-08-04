#ifndef APPTCPSOCKET
#define APPTCPSOCKET

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string.h>
#include <arpa/inet.h>

#include <json/json.h>
#include<fstream>
#include "../sharedbuffer/sharedbuffer.h"

const int SPLIC_PACK_SZIE = 1024 * 8;
const int RECV_BUFFER_SZIE = 2048;
const int LAST_BUFFER_SZIE = 1024 * 16;
const int DATA_HEAD = 0xAA33;
const int DATA_CHECK_VALUE = 0xCC;
const int SPEC_ARR_NUM = 2551;
#pragma pack(push) //保存对齐状态
#pragma pack(1)//设定为1 字节对齐
typedef struct _pack_header_t
{
	uint16_t header;//帧起始符，AA33
	uint16_t length;//数据长度, header + length + sepcnum + float1 + float2 + timestr

}pack_header_t;
#pragma pack(pop)//恢复对齐状态



class APPTCP
{
private:
public:
    APPTCP();
    ~APPTCP();
    void socketinit(int port);
    void dataReceiverThread();
    void parsePkg(char * buffer, int len);
    void parse_data_frame(char * buffer, int len);
    void writeFileJson(Json::Value Jsontemp);
public:
    int m_tcp_port ;
    int ser_tcp_lfd;
    struct sockaddr_in serverAddress;

    char recv_buffer[RECV_BUFFER_SZIE];
    char splic_pack_buffer[SPLIC_PACK_SZIE];

    int m_pkg_check;
    
    char last_loop_buffer[LAST_BUFFER_SZIE];
	unsigned int last_loop_length;
    int splic_index = 0;

    int16_t  spectrumArr[SPEC_ARR_NUM];
    // char   recv_temperature_tmp[4] ;
    // char   recv_pressure_tmp[4];

    float  recv_temperature[1];
    float   recv_pressure[1];

    char     timestr[24];   
    //线程同步
    std::queue<std::string> dataQueue;
    std::mutex dataMutex;
    std::condition_variable dataCondVar;
};


#endif
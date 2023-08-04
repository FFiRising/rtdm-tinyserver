#include <mutex>
#include <json/json.h>
#include <string.h>
#include <iostream>
using namespace std;
#define SPECNUM 2551
// 共享缓存
extern int sharedBuffer;
extern int16_t spec_share_buf[SPECNUM];
extern string Json_share_buf;

// 互斥锁
extern std::mutex bufferMutex;

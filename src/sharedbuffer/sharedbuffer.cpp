#include "sharedbuffer.h"

int sharedBuffer = 0;
std::mutex bufferMutex;
string Json_share_buf;
int16_t spec_share_buf[SPECNUM];
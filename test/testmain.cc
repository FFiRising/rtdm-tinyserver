#include <iostream>
#include "../src/log/log.h"


static rtdm::Logger::ptr g_logger = RTDM_LOG_ROOT();

int main(int argc, char** argv) {

    rtdm::Logger::ptr logger(new rtdm::Logger);
    logger->addAppender(rtdm::LogAppender::ptr(new rtdm::StdoutLogAppender));

    rtdm::FileLogAppender::ptr file_appender(new rtdm::FileLogAppender("./log.txt"));
    rtdm::LogFormatter::ptr fmt(new rtdm::LogFormatter("%d%T%p%T%f%T%r%T%l%T%m%n"));
    file_appender->setFormatter(fmt);
    file_appender->setLevel(rtdm::LogLevel::INFO);
    logger->addAppender(file_appender);

    std::cout << "hello rtdm log" << std::endl;

    RTDM_LOG_INFO(logger) << "test macro";
    RTDM_LOG_ERROR(logger) << "test macro error";

    RTDM_LOG_FMT_ERROR(logger, "test macro fmt error %s", "aa");

    auto l = rtdm::LoggerMgr::GetInstance()->getLogger("xx");
    RTDM_LOG_INFO(l) << "xxx";




   // 使用流式风格写日志
    RTDM_LOG_INFO(g_logger) << "hello logger stream";

    // 使用格式化写日志
    RTDM_LOG_FMT_INFO(g_logger, "%s", "hello logger format");
    return 0;
}

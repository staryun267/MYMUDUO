#include "Logger.h"
#include "Timestamp.h"

#include <iostream>

//获取Logger唯一实例对象
Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

//设置日志级别
void Logger::setLogLevel(int level)
{
    LogLevel_=level;
}

//写日志 [日志级别] 时间 : msg
void Logger::log(std::string msg)
{
    switch(LogLevel_)
    {
    case INFO:
        std::cout << "[INFO] ";
        break;
    case ERROR:
        std::cout << "[ERROR] ";
        break;
    case FATAL:
        std::cout << "[FATAL] ";
        break;
    case DEBUG:
        std::cout << "[DEBUG] ";
        break;
    default:
        break;
    }

    std::cout << Timestamp::now().toString() << ": " << msg << std::endl;
}
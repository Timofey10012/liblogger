#pragma once

#include "logger/logger.hpp"
#include <mutex>

class SocketLogger : public Logger {
public:
    SocketLogger(const std::string& ip, uint16_t port, LogLevel logLevel);
    ~SocketLogger() override;

    void log(LogLevel level, const std::string &message) override;
    void setLogLevel(LogLevel level) override;

private:
    int socketFd_ = -1;
    LogLevel level_;
    std::mutex mutex_;
};
#pragma once

#include <string>
#include <ctime>
#include <sstream>
#include <chrono>
#include <iomanip>

#include "log_level.hpp"

class Logger {
    public:
    virtual ~Logger() = default;

    virtual void log(LogLevel level, const std::string& message) = 0;
    virtual void setLogLevel(LogLevel level) = 0;
};

inline std::string getCurrentTime() {
    auto now = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now()
    );

    std::tm time = *std::localtime(&now);

    std::ostringstream oss;
    oss << std::put_time(&time, "%Y-%m-%d %H:%M:%S");

    return oss.str();
}
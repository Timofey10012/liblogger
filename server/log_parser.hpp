#pragma once

#include <chrono>
#include <string>

enum class LogLevel {
    Info,
    Warning,
    Error
};

struct ParsedLog {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string msg;
};

ParsedLog parseLog(const std::string& log);
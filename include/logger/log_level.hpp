#pragma once

enum class LogLevel {
    Info = 0,
    Warning = 1,
    Error = 2,
};

inline std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARNING";
        case LogLevel::Error:
            return "ERROR";
    }
    return "UNKNOWN";
};
#include "logger/file_logger.hpp"

FileLogger::FileLogger(const std::string& fileName, LogLevel level)
    : file_(fileName, std::ios::app), level_(level)
{
    if (!file_.is_open()) {
        throw std::runtime_error("Unable to open file " + fileName);
    }

}

FileLogger::~FileLogger() {
    if (file_.is_open()) {
        file_.close();
    }
}

void FileLogger::log(LogLevel level, const std::string &message) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (level < level_) {
        return;
    }

    file_ << "[" << getCurrentTime() << "]"
    << "[" << logLevelToString(level) << "] "
    << message << "\n";

    file_.flush();

    if (!file_) {
        throw std::runtime_error("Failed to write to log file");
    }
}

void FileLogger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

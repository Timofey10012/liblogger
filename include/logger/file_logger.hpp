#pragma once

#include <fstream>
#include <mutex>

#include "logger.hpp"

class FileLogger : public Logger {
  public:
  FileLogger(const std::string& fileName, LogLevel logLevel);
  ~FileLogger() override;

  void log(LogLevel level, const std::string &message) override;
  void setLogLevel(LogLevel level) override;

  private:
  std::ofstream file_;
  LogLevel level_;
  std::mutex mutex_;
};
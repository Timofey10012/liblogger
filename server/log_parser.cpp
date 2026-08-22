#include "log_parser.hpp"

#include <stdexcept>
#include <ctime>
#include <iomanip>
#include <sstream>

ParsedLog parseLog(const std::string& log) {
    size_t firstEnd = log.find(']');
    if (firstEnd == std::string::npos) {
        throw std::runtime_error("Invalid log format");
    }

    size_t secondStart = log.find('[', firstEnd);
    size_t secondEnd = log.find(']', secondStart);

    if (secondStart == std::string::npos ||
        secondEnd == std::string::npos) {
        throw std::runtime_error("Invalid log format");
        }

    ParsedLog result;

    std::tm tm{};

    std::string timestamp = log.substr(1, firstEnd - 1);

    std::istringstream ss(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

    if (ss.fail()) {
        throw std::runtime_error("Invalid timestamp");
    }

    result.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    std::string levelStr =
        log.substr(secondStart + 1, secondEnd - secondStart - 1);

    if (levelStr == "INFO") { result.level = LogLevel::Info; }
    else if (levelStr == "WARNING") { result.level = LogLevel::Warning; }
    else if (levelStr == "ERROR") { result.level = LogLevel::Error; }
    else {
        throw std::runtime_error("Unknown log level");
    }

    result.msg = log.substr(secondEnd + 2);

    return result;
}
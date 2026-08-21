#include <iostream>
#include <optional>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>
#include <csignal>

#include "logger/file_logger.hpp"

struct LogMessage {
    LogMessage(std::string message, LogLevel level)
        : message(message), level(level) {}
    std::string message;
    LogLevel level;
};

std::atomic<bool> running {true};
std::queue<LogMessage> logMessages;
std::mutex logMessagesMutex;
std::condition_variable logMessagesCV;

void signalHandler(int signal) {
    if (signal == SIGINT) {
        running = false;
    }
}

std::optional<LogLevel> parseLogLevel(const std::string& level);
void worker(FileLogger& logger);

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);

    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <log_file> <log_level>\n";
        return 1;
    }

    try {
        auto level_current = parseLogLevel(argv[2]);
        if (!level_current) {
            throw std::runtime_error("Unknown log level");
        }
        LogLevel logger_level = *level_current;

        std::string log_file = argv[1];

        FileLogger logger(log_file, logger_level);

        std::thread t(worker, std::ref(logger));

        while (running) {
            std::string message;
            while (true) {
                std::cout << "Message: ";
                std::getline(std::cin, message);

                if (!running) break;

                if (message.empty()) {
                    std::cout << "Message cannot be empty\n";
                    continue;
                }

                break;
            }

            if (!running) break;

            LogLevel log_level = logger_level;
            while (true) {
                std::string log_level_str;
                std::cout << "Level (info/warning/error, Enter for default): " ;
                std::getline(std::cin, log_level_str);

                if (!running) break;

                if (log_level_str.empty()) {
                    break;
                }

                level_current = parseLogLevel(log_level_str);
                if (!level_current) {
                    std::cout << "Error: Unknown log level\n";
                    continue;
                }

                log_level = *level_current;
                break;
            }

            if (!running) break;

            {
                std::lock_guard<std::mutex> lock(logMessagesMutex);
                logMessages.push(LogMessage(message, log_level));
            }
            logMessagesCV.notify_one();
        }


        logMessagesCV.notify_one();
        t.join();

    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }
}

std::optional<LogLevel> parseLogLevel(const std::string& level) {
    if (level == "Info" || level == "info" || level == "INFO") return LogLevel::Info;
    if (level == "Warning" || level == "warning" || level == "WARNING") return LogLevel::Warning;
    if (level == "Error" || level == "error" || level == "ERROR") return LogLevel::Error;

    return std::nullopt;
}

void worker(FileLogger& logger) {
    while (true) {
        std::unique_lock<std::mutex> lock(logMessagesMutex);

        logMessagesCV.wait(lock, [&]()
            { return !logMessages.empty() || !running; });

        if (logMessages.empty() && !running) {
            return;
        }

        auto msg = logMessages.front();
        logMessages.pop();

        lock.unlock();
        try {
            logger.log(msg.level, msg.message);
        } catch (const std::exception& e) {
            std::cout << "Logging error: " << e.what() << std::endl;
        }
    }
}
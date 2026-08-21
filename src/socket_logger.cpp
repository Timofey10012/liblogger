#include "logger/socket_logger.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdexcept>

SocketLogger::SocketLogger(const std::string& ip, uint16_t port, LogLevel level)
    : level_(level)
{
    socketFd_ = socket(AF_INET, SOCK_STREAM, 0);

    if (socketFd_ == -1) {
        throw std::runtime_error("Failed to create socket");
    }

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &address.sin_addr) == -1) {
        close(socketFd_);
        throw std::runtime_error("Failed to parse ip address");
    }

    if (connect(socketFd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1) {
        close(socketFd_);
        throw std::runtime_error("Failed to connect to server");
    }

}

SocketLogger::~SocketLogger() {
    if (socketFd_ != -1) {
        close(socketFd_);
    }
}

void SocketLogger::log(LogLevel level, const std::string &message) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (level < level_) {
        return;
    }

    std::string data = "[" + getCurrentTime() + "]"
    + "[" + logLevelToString(level) + "] "
    + message + "\n";

    size_t totalSent = 0;

    while (totalSent < data.size()) {
        ssize_t sent = send(socketFd_,
            data.data() + totalSent,
            data.size() - totalSent,
            0
        );

        if (sent <= 0) {
            throw std::runtime_error("Failed to send message");
        }

        totalSent += sent;
    }

}

void SocketLogger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

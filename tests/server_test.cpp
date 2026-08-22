#include <iostream>
#include <thread>

#include "logger/socket_logger.hpp"

int main(int argc, char *argv[]) {

    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    try {
        std::string portStr(argv[1]);
        uint16_t port = stoi(portStr);

        SocketLogger logger("127.0.0.1", port, LogLevel::Info);

        logger.log(LogLevel::Info, "Hello World");
        logger.log(LogLevel::Warning, "Hi");
        logger.log(LogLevel::Error, "Example error message");

        logger.log(LogLevel::Info, "Hello");

        std::this_thread::sleep_for(std::chrono::seconds(6));

        logger.log(LogLevel::Info, "Hello");

        std::this_thread::sleep_for(std::chrono::seconds(6));

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <filesystem>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>

#include "logger/file_logger.hpp"
#include "logger/socket_logger.hpp"

void testMessageIsWritten() {
    const std::string fileName = "testMessageIsWritten.log";
    std::filesystem::remove(fileName);

    FileLogger logger(fileName, LogLevel::Info);

    logger.log(LogLevel::Info, "Hello World");

    std::ifstream logFile(fileName);

    std::string fileContents;
    if (!std::getline(logFile, fileContents)) {
        throw std::runtime_error("testMessageIsWritten error: Log file is empty");
    }
    if (fileContents.find("Hello World") == std::string::npos) {
        throw std::runtime_error("testMessageIsWritten error: Message was not written");
    }
}

void testMessageBelowLevelIsIgnored() {
    const std::string fileName = "testMessageBelowLevelIsIgnored.log";
    std::filesystem::remove(fileName);

    FileLogger logger(fileName, LogLevel::Warning);

    logger.log(LogLevel::Info, "Hello World");

    std::ifstream logFile(fileName);

    std::string fileContents;

    if (!logFile.is_open()) {
        throw std::runtime_error("testMessageBelowLevelIsIgnored error: Log file was not created");
    }

    if (std::getline(logFile, fileContents)) {
        throw std::runtime_error("testMessageBelowLevelIsIgnored error: Message below level was written");
    }
}

void testSetLogLevel() {
    const std::string fileName = "testSetLogLevel.log";
    std::filesystem::remove(fileName);

    FileLogger logger(fileName, LogLevel::Warning);
    logger.log(LogLevel::Info, "Ignored");

    logger.setLogLevel(LogLevel::Info);
    logger.log(LogLevel::Info, "Written");

    std::ifstream logFile(fileName);

    std::string fileContents;
    bool foundIgnored = false;
    bool foundWritten = false;

    while (std::getline(logFile, fileContents)) {
        if (fileContents.find("Ignored") != std::string::npos) {
            foundIgnored = true;
        }
        if (fileContents.find("Written") != std::string::npos) {
            foundWritten = true;
        }
    }

    if (foundIgnored) {
        throw std::runtime_error("testSetLogLevel error: Ignored message was written");
    }

    if (!foundWritten) {
        throw std::runtime_error("testSetLogLevel error: Message was not written after changing level");
    }
}

void testErrorOpeningLogFile() {
    const std::string fileName = "/this_directory_should_not_exist/test.log";

    try {
        FileLogger logger(fileName, LogLevel::Info);
    } catch (const std::runtime_error& e) {
        return;
    }

    throw std::runtime_error("testErrorOpeningLogFile error: FileLogger did not throw an exception");
}

void testSocketLogger() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket == -1) {
        throw std::runtime_error("testSocketLogger error: Failed to create server socket");
    }

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0);

    if (bind(
        serverSocket,
        reinterpret_cast<sockaddr*>(&address),
        sizeof(address)) == -1) {
        close(serverSocket);
        throw std::runtime_error("testSocketLogger error: Failed to bind server socket");
    }

    if (listen(serverSocket, 1) == -1) {
        close(serverSocket);
        throw std::runtime_error("testSocketLogger error: Failed to listen");
    }

    socklen_t addressSize = sizeof(address);

    if (getsockname(
        serverSocket,
        reinterpret_cast<sockaddr*>(&address),
        &addressSize) == -1) {
        close(serverSocket);
        throw std::runtime_error("testSocketLogger error: Failed to get server port");
    }

    uint16_t port = ntohs(address.sin_port);

    std::string receivedMessage;

    std::thread serverThread([&]() {
        int clientSocket = accept(serverSocket, nullptr, nullptr);

        if (clientSocket == -1) {
            return;
        }

        char buffer[1024];

        ssize_t received = recv(
            clientSocket,
            buffer,
            sizeof(buffer),
            0
        );

        if (received > 0) {
            receivedMessage.assign(buffer, received);
        }

        close(clientSocket);
    });

    SocketLogger logger("127.0.0.1", port, LogLevel::Info);

    logger.log(LogLevel::Info, "Hello World");

    serverThread.join();
    close(serverSocket);

    if (receivedMessage.find("[INFO]") == std::string::npos) {
        throw std::runtime_error("testSocketLogger error: Log level was not sent");
    }

    if (receivedMessage.find("Hello World") == std::string::npos) {
        throw std::runtime_error("testSocketLogger error: Message was not sent");
    }
}

int main() {
    try {
        testMessageIsWritten();
        testMessageBelowLevelIsIgnored();
        testSetLogLevel();
        testErrorOpeningLogFile();
        testSocketLogger();

        std::cout << "All tests passed!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test Failed: " << e.what() << std::endl;
        return 1;
    }
}
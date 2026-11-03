#include <iostream>
#include <stdexcept>
#include <fstream>
#include <filesystem>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <csignal>
#include <functional>
#include <sys/wait.h>

#include "logger/file_logger.hpp"
#include "logger/socket_logger.hpp"

// Per-process directory in the system temp dir: the tests don't depend on
// the working directory and leave nothing behind.
class TestDirectory {
public:
    TestDirectory()
        : path_(std::filesystem::temp_directory_path() / ("liblogger_tests_" + std::to_string(getpid())))
    {
        std::filesystem::create_directories(path_);
    }

    ~TestDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TestDirectory(const TestDirectory&) = delete;
    TestDirectory& operator=(const TestDirectory&) = delete;

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

std::string testLogPath(const std::string& name) {
    static const TestDirectory directory;
    return (directory.path() / name).string();
}

void testMessageIsWritten() {
    const std::string fileName = testLogPath("testMessageIsWritten.log");
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
    const std::string fileName = testLogPath("testMessageBelowLevelIsIgnored.log");
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
    const std::string fileName = testLogPath("testSetLogLevel.log");
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

struct ListeningSocket {
    int fd;
    uint16_t port;
};

ListeningSocket createListeningSocket() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket == -1) {
        throw std::runtime_error("Failed to create server socket");
    }

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0);

    socklen_t addressSize = sizeof(address);

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1 ||
        listen(serverSocket, 1) == -1 ||
        getsockname(serverSocket, reinterpret_cast<sockaddr*>(&address), &addressSize) == -1) {
        close(serverSocket);
        throw std::runtime_error("Failed to set up server socket");
    }

    return {serverSocket, ntohs(address.sin_port)};
}

// Accepts the logger's connection and closes it at once,
// leaving the logger with a connection whose peer is gone.
void acceptAndClose(int serverSocket) {
    int clientSocket = accept(serverSocket, nullptr, nullptr);

    if (clientSocket == -1) {
        throw std::runtime_error("Failed to accept client connection");
    }

    close(clientSocket);
}

// The first send after the peer closed may still succeed
// so keep logging until the logger reports failure.
bool logUntilFailure(SocketLogger& logger, LogLevel level, const std::string& message) {
    for (int i = 0; i < 50; ++i) {
        try {
            logger.log(level, message);
        } catch (const std::runtime_error&) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

// Runs the scenario in a child process, so a fatal signal such as SIGPIPE
// fails only this test instead of killing the whole test run.
void runInChild(const std::string& testName, const std::function<void()>& scenario) {
    pid_t pid = fork();

    if (pid == -1) {
        throw std::runtime_error(testName + " error: fork failed");
    }

    if (pid == 0) {
        try {
            scenario();
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            _exit(1);
        }
        _exit(0);
    }

    int status = 0;
    waitpid(pid, &status, 0);

    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGPIPE) {
        throw std::runtime_error(testName + " error: process was killed by SIGPIPE");
    }

    if (WIFSIGNALED(status)) {
        throw std::runtime_error(testName + " error: process was killed by signal " + std::to_string(WTERMSIG(status)));
    }

    if (WEXITSTATUS(status) != 0) {
        throw std::runtime_error(testName + " error: scenario failed");
    }
}

void testSocketLoggerPeerClosedThrows() {
    runInChild("testSocketLoggerPeerClosedThrows", []() {
        ListeningSocket server = createListeningSocket();
        SocketLogger logger("127.0.0.1", server.port, LogLevel::Info);
        acceptAndClose(server.fd);

        if (!logUntilFailure(logger, LogLevel::Info, "Hello World")) {
            throw std::runtime_error("testSocketLoggerPeerClosedThrows error: log did not throw after the peer closed");
        }
    });
}

void testSocketLoggerRepeatedLogAfterPeerClosed() {
    runInChild("testSocketLoggerRepeatedLogAfterPeerClosed", []() {
        ListeningSocket server = createListeningSocket();
        SocketLogger logger("127.0.0.1", server.port, LogLevel::Info);
        acceptAndClose(server.fd);

        if (!logUntilFailure(logger, LogLevel::Info, "Hello World")) {
            throw std::runtime_error("testSocketLoggerRepeatedLogAfterPeerClosed error: log did not throw after the peer closed");
        }

        for (int i = 0; i < 5; ++i) {
            try {
                logger.log(LogLevel::Info, "Hello again");
            } catch (const std::runtime_error&) {
                continue;
            }
            throw std::runtime_error("testSocketLoggerRepeatedLogAfterPeerClosed error: log succeeded on a broken connection");
        }
    });
}

void testSocketLoggerBelowLevelAfterPeerClosedIsSilent() {
    runInChild("testSocketLoggerBelowLevelAfterPeerClosedIsSilent", []() {
        ListeningSocket server = createListeningSocket();
        SocketLogger logger("127.0.0.1", server.port, LogLevel::Warning);
        acceptAndClose(server.fd);

        // Let the peer's close reach the logger's socket.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Filtered messages must never reach send(), so no error and no signal.
        for (int i = 0; i < 20; ++i) {
            logger.log(LogLevel::Info, "Ignored");
        }
    });
}

void testSocketLoggerLargeMessagePeerClosesMidway() {
    runInChild("testSocketLoggerLargeMessagePeerClosesMidway", []() {
        ListeningSocket server = createListeningSocket();
        SocketLogger logger("127.0.0.1", server.port, LogLevel::Info);

        std::thread serverThread([&]() {
            int clientSocket = accept(server.fd, nullptr, nullptr);

            if (clientSocket == -1) {
                return;
            }

            char buffer[1024];
            recv(clientSocket, buffer, sizeof(buffer), 0);
            close(clientSocket);
        });

        const std::string bigMessage(16 * 1024 * 1024, 'x');
        bool failed = false;

        try {
            logger.log(LogLevel::Info, bigMessage);
        } catch (const std::runtime_error&) {
            failed = true;
        }

        serverThread.join();

        if (!failed) {
            throw std::runtime_error("testSocketLoggerLargeMessagePeerClosesMidway error: log did not throw when the peer closed mid-message");
        }
    });
}

volatile std::sig_atomic_t sigpipeCount = 0;

void sigpipeHandler(int) {
    sigpipeCount = sigpipeCount + 1;
}

void testSocketLoggerLeavesHostSigpipeHandlerAlone() {
    runInChild("testSocketLoggerLeavesHostSigpipeHandlerAlone", []() {
        struct sigaction action {};
        action.sa_handler = sigpipeHandler;
        sigemptyset(&action.sa_mask);
        sigaction(SIGPIPE, &action, nullptr);

        ListeningSocket server = createListeningSocket();
        SocketLogger logger("127.0.0.1", server.port, LogLevel::Info);
        acceptAndClose(server.fd);

        logUntilFailure(logger, LogLevel::Info, "Hello World");

        struct sigaction current {};
        sigaction(SIGPIPE, nullptr, &current);

        if (current.sa_handler != sigpipeHandler) {
            throw std::runtime_error("testSocketLoggerLeavesHostSigpipeHandlerAlone error: library changed the host's SIGPIPE handler");
        }

        if (sigpipeCount != 0) {
            throw std::runtime_error("testSocketLoggerLeavesHostSigpipeHandlerAlone error: SIGPIPE was raised");
        }
    });
}

int runTest(const char* name, void (*test)()) {
    try {
        test();
        std::cout << "[ OK ] " << name << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] " << name << ": " << e.what() << std::endl;
        return 1;
    }
}

int main() {
    int failed = 0;

    failed += runTest("testMessageIsWritten", testMessageIsWritten);
    failed += runTest("testMessageBelowLevelIsIgnored", testMessageBelowLevelIsIgnored);
    failed += runTest("testSetLogLevel", testSetLogLevel);
    failed += runTest("testErrorOpeningLogFile", testErrorOpeningLogFile);
    failed += runTest("testSocketLogger", testSocketLogger);
    failed += runTest("testSocketLoggerPeerClosedThrows", testSocketLoggerPeerClosedThrows);
    failed += runTest("testSocketLoggerRepeatedLogAfterPeerClosed", testSocketLoggerRepeatedLogAfterPeerClosed);
    failed += runTest("testSocketLoggerBelowLevelAfterPeerClosedIsSilent", testSocketLoggerBelowLevelAfterPeerClosedIsSilent);
    failed += runTest("testSocketLoggerLargeMessagePeerClosesMidway", testSocketLoggerLargeMessagePeerClosesMidway);
    failed += runTest("testSocketLoggerLeavesHostSigpipeHandlerAlone", testSocketLoggerLeavesHostSigpipeHandlerAlone);

    if (failed != 0) {
        std::cerr << failed << " test(s) failed" << std::endl;
        return 1;
    }

    std::cout << "All tests passed!\n";
    return 0;
}
#include "log_parser.hpp"
#include "statistics.hpp"

#include <iostream>
#include <exception>
#include <optional>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <csignal>
#include <cerrno>
#include <string>

bool running = true;

void signalHandler(int signal) {
    if (signal == SIGINT) {
        running = false;
    }
}

std::optional<int> parseInt(const std::string& s);

int main(int argc, char *argv[]) {
    signal(SIGINT, signalHandler);

    if (argc != 4) {
        std::cout << "Usage: " << argv[0] << " <port> <messages_count> <timeout_seconds>\n";
        return 1;
    }

    auto portOpt = parseInt(argv[1]);

    if (!portOpt) {
        std::cout << "Port must be a number\n";
        return 1;
    }

    if (*portOpt < 1 || *portOpt > 65535) {
        std::cout << "Invalid port\n";
        return 1;
    }

    uint16_t port = static_cast<uint16_t>(*portOpt);

    auto messageCountOpt = parseInt(argv[2]);

    if (!messageCountOpt || *messageCountOpt <= 0) {
        std::cout << "Messages count must be a positive number\n";
        return 1;
    }

    auto timeoutOpt = parseInt(argv[3]);

    if (!timeoutOpt || *timeoutOpt <= 0) {
        std::cout << "Timeout must be a positive number\n";
        return 1;
    }

    int messageCount = *messageCountOpt;
    int timeout = *timeoutOpt;

    try {
        int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

        if (serverSocket == -1) {
            throw std::runtime_error("Failed to create server socket");
        }

        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port);

        if (bind(
            serverSocket,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) == -1) {
            close(serverSocket);
            throw std::runtime_error("Failed to bind server socket");
        }

        if (listen(serverSocket, 1) == -1) {
            close(serverSocket);
            throw std::runtime_error("Failed to listen on server socket");
        }

        pollfd serverPfd{};
        serverPfd.fd = serverSocket;
        serverPfd.events = POLLIN;

        while (running) {
            int result = poll(&serverPfd, 1, 1000);

            if (result == -1) {
                if (errno == EINTR) {
                    break;
                }

                throw std::runtime_error("Poll failed");
            }

            if (result == 0) {
                continue;
            }

            if (serverPfd.revents & POLLIN) {
                break;
            }
        }

        if (!running) {
            close(serverSocket);
            return 0;
        }

        int clientSocket = accept(serverSocket, nullptr, nullptr);

        if (clientSocket == -1) {
            close(serverSocket);
            throw std::runtime_error("Failed to accept client connection");
        }

        pollfd clientPfd{};
        clientPfd.fd = clientSocket;
        clientPfd.events = POLLIN;

        std::string buffer;
        char chunk[1024];

        Statistics statistics;
        bool statisticsChanged = false;

        while (running) {
            int result = poll(&clientPfd, 1, timeout * 1000);

            if (result == -1) {
                if (errno == EINTR) {
                    break;
                }

                throw std::runtime_error("Poll failed");
            }

            if (result == 0) {
                if (statisticsChanged) {
                    statistics.print();
                    statisticsChanged = false;
                }
                continue;
            }

            if (clientPfd.revents & POLLIN) {
                ssize_t received = recv(
                   clientSocket,
                   chunk,
                   sizeof(chunk),
                   0);

                if (received > 0 ) {
                    buffer.append(chunk, received);

                    size_t newLinePos;

                    while ((newLinePos = buffer.find('\n')) != std::string::npos) {
                        std::string msg = buffer.substr(0, newLinePos);
                        buffer.erase(0, newLinePos + 1);

                        std::cout << msg << std::endl;

                        statisticsChanged = true;

                        ParsedLog log = parseLog(msg);

                        statistics.addMessage(log);

                        if (statistics.getTotalMessagesCount() % messageCount == 0) {
                            statistics.print();
                            statisticsChanged = false;
                        }
                    }
                }
                else if (received == 0) {
                    std::cout << "Client disconnected\n";
                    break;
                }
                else {
                    throw std::runtime_error("Failed to receive message");
                }
            }
        }

        close(clientSocket);
        close(serverSocket);

        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}

std::optional<int> parseInt(const std::string& s) {
    try {
        size_t pos = 0;
        int value = stoi(s, &pos);

        if (pos != s.size()) {
            return std::nullopt;
        }

        return value;
    } catch (const std::exception& e) {
        return std::nullopt;
    }
}
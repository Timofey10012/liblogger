#pragma once

#include "log_parser.hpp"

#include <deque>
#include <chrono>

class Statistics {
public:
    void addMessage(const ParsedLog& log);
    void print();

    size_t getTotalMessagesCount();

private:
    size_t totalMessagesCount = 0;
    size_t infoMessagesCount = 0;
    size_t warningMessagesCount = 0;
    size_t errorMessagesCount = 0;

    size_t totalLength = 0;
    size_t maxLength = 0;
    size_t minLength = 0;

    std::deque<std::chrono::system_clock::time_point> messageTimes;
};
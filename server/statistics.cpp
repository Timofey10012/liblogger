#include "statistics.hpp"

#include <iostream>

void Statistics::addMessage(const ParsedLog &log) {
    ++totalMessagesCount;

    messageTimes.push_back(log.timestamp);

    switch (log.level) {
        case LogLevel::Info:
            ++infoMessagesCount;
            break;
        case LogLevel::Warning:
            ++warningMessagesCount;
            break;
        case LogLevel::Error:
            ++errorMessagesCount;
            break;
    }

    if (totalMessagesCount == 1) {
        maxLength = log.msg.size();
        minLength = log.msg.size();
    }

    maxLength = std::max(maxLength, log.msg.size());
    minLength = std::min(minLength, log.msg.size());

    totalLength += log.msg.size();
}

void Statistics::print() {
    auto oneHourAgo = std::chrono::system_clock::now() - std::chrono::hours(1);

    while (!messageTimes.empty() && messageTimes.front() < oneHourAgo) {
        messageTimes.pop_front();
    }

    std::cout << "Total messages: " << totalMessagesCount
    << "\nInfo messages: " << infoMessagesCount
    << "\nWarning messages: " << warningMessagesCount
    << "\nError messages: " << errorMessagesCount
    << "\nMessages last hour: " << messageTimes.size()
    << "\n\nMin length message: " << minLength
    << "\nMax length message: " << maxLength
    << "\nAverage length message: " << static_cast<double>(totalLength) / totalMessagesCount
    << std::endl;
}

size_t Statistics::getTotalMessagesCount() {
    return totalMessagesCount;
}

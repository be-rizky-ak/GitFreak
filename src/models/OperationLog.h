#pragma once

#include <mutex>
#include <string>
#include <vector>

class OperationLog
{
public:
    void append(
        bool isError,
        const std::string& text);

    std::vector<std::string> consume();

private:
    std::mutex m_mutex;

    std::vector<std::string> m_entries;
};
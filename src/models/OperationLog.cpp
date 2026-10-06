#include "OperationLog.h"

void OperationLog::append(bool isError, const std::string& text)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (isError)
    {
        m_entries.push_back("[stderr] " + text);
    }
    else
    {
        m_entries.push_back(text);
    }
}

std::vector<std::string> OperationLog::consume()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> entries;

    entries.swap(m_entries);

    return entries;
}
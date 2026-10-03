#pragma once

#include <string>

enum class OperationStatus
{
    Running,
    Success,
    Failed
};

struct OperationState
{
    OperationStatus status =
        OperationStatus::Running;

    std::string title;
    std::string command;

    int exitCode = -1;
};
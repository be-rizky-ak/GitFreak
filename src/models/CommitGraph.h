#pragma once

#include "Commit.h"

#include <vector>

struct CommitGraphNode
{
    Commit commit;

    int lane = 0;
    bool hasUpperLine = false;
    bool isUpperLineActive = true;
    bool isActive = true;

    std::vector<int> parentLanes;
    std::vector<bool> parentLanesActive;
    std::vector<bool> parentLanesInActive;

    std::vector<int> activeLanes;
    std::vector<bool> activeLanesActive;
};

class CommitGraph
{
public:
    static std::vector<CommitGraphNode> build(
        const std::vector<Commit>& commits, const std::string& activeHeadHash = "");
};
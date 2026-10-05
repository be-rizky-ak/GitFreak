#pragma once

#include "Commit.h"

#include <vector>

struct CommitGraphNode
{
    Commit commit;

    int lane = 0;

    std::vector<int> parentLanes;
};

class CommitGraph
{
public:
    static std::vector<CommitGraphNode> build(
        const std::vector<Commit>& commits);
};
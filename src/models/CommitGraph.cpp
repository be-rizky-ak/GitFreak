#include "CommitGraph.h"

#include <algorithm>
#include <string>
#include <vector>

std::vector<CommitGraphNode> CommitGraph::build(const std::vector<Commit>& commits)
{
    std::vector<CommitGraphNode> result;

    result.reserve(commits.size());

    if (commits.empty())
    {
        return result;
    }

    // Each lane contains the commit hash
    // expected to appear next in that lane.
    std::vector<std::string> lanes;

    auto findLane = [&lanes](const std::string& hash) -> int
    {
        for (int lane = 0; lane < static_cast<int>(lanes.size()); ++lane)
        {
            if (lanes[lane] == hash)
            {
                return lane;
            }
        }

        return -1;
    };

    auto createLane = [&lanes](const std::string& hash) -> int
    {
        for (int lane = 0; lane < static_cast<int>(lanes.size()); ++lane)
        {
            if (lanes[lane].empty())
            {
                lanes[lane] = hash;

                return lane;
            }
        }

        lanes.push_back(hash);

        return static_cast<int>(lanes.size() - 1);
    };

    for (const Commit& commit : commits)
    {
        CommitGraphNode node;

        node.commit = commit;

        // Find the lane where this commit is expected.
        int currentLane = findLane(commit.hash);

        // If this commit isn't currently expected
        // by any lane, create a new lane.
        if (currentLane < 0)
        {
            currentLane = createLane(commit.hash);
        }

        node.lane = currentLane;

        // This commit has now been consumed.
        lanes[currentLane].clear();

        // Assign lanes to all parents.
        for (std::size_t parentIndex = 0; parentIndex < commit.parents.size(); ++parentIndex)
        {
            const std::string& parent = commit.parents[parentIndex];

            if (parent.empty())
            {
                continue;
            }

            int parentLane = findLane(parent);

            if (parentLane < 0)
            {
                if (parentIndex == 0)
                {
                    // First parent continues on the
                    // current lane.
                    parentLane = currentLane;

                    lanes[parentLane] = parent;
                }
                else
                {
                    // Additional parents create
                    // additional lanes.
                    parentLane = createLane(parent);
                }
            }

            if (std::find(node.parentLanes.begin(), node.parentLanes.end(), parentLane) ==
                node.parentLanes.end())
            {
                node.parentLanes.push_back(parentLane);
            }
        }

        // -------------------------------------------------
        // Capture lanes that remain active after this row.
        // -------------------------------------------------

        for (int lane = 0; lane < static_cast<int>(lanes.size()); ++lane)
        {
            if (!lanes[lane].empty())
            {
                node.activeLanes.push_back(lane);
            }
        }

        result.push_back(std::move(node));
    }

    return result;
}
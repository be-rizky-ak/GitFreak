#include "CommitGraph.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

std::vector<CommitGraphNode> CommitGraph::build(
    const std::vector<Commit>& commits, const std::string& activeHeadHash)
{
    std::vector<CommitGraphNode> result;

    result.reserve(commits.size());

    if (commits.empty())
    {
        return result;
    }

    // Map commit hash to Commit pointer for fast lookup
    std::unordered_map<std::string, const Commit*> commitMap;
    commitMap.reserve(commits.size());
    for (const Commit& commit : commits)
    {
        commitMap[commit.hash] = &commit;
    }

    // Determine starting active commit hash (active branch tip / HEAD)
    std::string startHash = activeHeadHash;
    if (startHash.empty() || commitMap.find(startHash) == commitMap.end())
    {
        startHash = commits[0].hash;
    }

    // Traverse ancestors from active HEAD to find all commits merged into active branch
    std::unordered_set<std::string> activeCommits;
    if (commitMap.find(startHash) != commitMap.end())
    {
        std::vector<std::string> queue = {startHash};
        activeCommits.insert(startHash);

        while (!queue.empty())
        {
            std::string currentHash = queue.back();
            queue.pop_back();

            auto it = commitMap.find(currentHash);
            if (it != commitMap.end())
            {
                for (const std::string& parentHash : it->second->parents)
                {
                    if (!parentHash.empty() && activeCommits.find(parentHash) == activeCommits.end())
                    {
                        activeCommits.insert(parentHash);
                        queue.push_back(parentHash);
                    }
                }
            }
        }
    }

    // Each lane contains the commit hash expected to appear next in that lane.
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
        node.isActive = (activeCommits.find(commit.hash) != activeCommits.end());

        // Record lanes that existed before processing this commit
        std::vector<bool> laneExistedBefore(lanes.size(), false);
        for (std::size_t i = 0; i < lanes.size(); ++i)
        {
            if (!lanes[i].empty())
            {
                laneExistedBefore[i] = true;
            }
        }

        // Find the lane where this commit is expected.
        int currentLane = findLane(commit.hash);

        if (currentLane >= 0)
        {
            node.hasUpperLine = true;
        }
        else
        {
            node.hasUpperLine = false;
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
                    // First parent continues on the current lane.
                    parentLane = currentLane;
                    lanes[parentLane] = parent;
                }
                else
                {
                    // Additional parents create additional lanes.
                    parentLane = createLane(parent);
                }
            }

            if (std::find(node.parentLanes.begin(), node.parentLanes.end(), parentLane) ==
                node.parentLanes.end())
            {
                node.parentLanes.push_back(parentLane);
                bool parentIsActive = node.isActive && (activeCommits.find(parent) != activeCommits.end());
                node.parentLanesActive.push_back(parentIsActive);
            }
        }

        // -------------------------------------------------
        // Capture lanes that remain active after this row.
        // -------------------------------------------------

        for (int lane = 0; lane < static_cast<int>(lanes.size()); ++lane)
        {
            if (!lanes[lane].empty() && lane != node.lane)
            {
                // If this lane is a parent lane of node, only include it in activeLanes if it existed
                // before this commit (i.e. was active above this row).
                // If it was newly created at this commit, it starts at 3/4 height (rendered by parentLanes)
                // and should NOT be rendered from top in activeLanes.
                bool isParentLane = (std::find(node.parentLanes.begin(), node.parentLanes.end(), lane) !=
                                     node.parentLanes.end());
                bool existedBefore = (lane < static_cast<int>(laneExistedBefore.size()) && laneExistedBefore[lane]);

                if (!isParentLane || existedBefore)
                {
                    node.activeLanes.push_back(lane);
                    bool laneIsActive = (activeCommits.find(lanes[lane]) != activeCommits.end());
                    node.activeLanesActive.push_back(laneIsActive);
                }
            }
        }

        result.push_back(std::move(node));
    }

    return result;
}

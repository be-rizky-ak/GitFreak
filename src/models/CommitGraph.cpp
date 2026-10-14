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

    // -------------------------------------------------
    // PASS 1: Pre-calculate lane assignment for each commit
    // -------------------------------------------------
    std::unordered_map<std::string, int> commitLanes;

    auto assignFirstParentChain = [&](const std::string& firstHash, int lane) {
        std::string current = firstHash;
        while (!current.empty())
        {
            auto it = commitMap.find(current);
            if (it == commitMap.end())
            {
                break;
            }
            if (commitLanes.find(current) != commitLanes.end())
            {
                break;
            }
            commitLanes[current] = lane;

            if (!it->second->parents.empty())
            {
                current = it->second->parents[0];
            }
            else
            {
                current.clear();
            }
        }
    };

    if (commitMap.find(startHash) != commitMap.end())
    {
        assignFirstParentChain(startHash, 0);
    }
    else
    {
        assignFirstParentChain(commits[0].hash, 0);
    }

    int nextLane = 1;
    for (const Commit& commit : commits)
    {
        if (commitLanes.find(commit.hash) == commitLanes.end())
        {
            assignFirstParentChain(commit.hash, nextLane++);
        }

        for (std::size_t p = 1; p < commit.parents.size(); ++p)
        {
            const std::string& parentHash = commit.parents[p];
            if (!parentHash.empty() && commitLanes.find(parentHash) == commitLanes.end())
            {
                assignFirstParentChain(parentHash, nextLane++);
            }
        }
    }

    // -------------------------------------------------
    // PASS 2: Build graph nodes row-by-row
    // -------------------------------------------------
    struct LaneState
    {
        std::string targetHash;
        bool isActive = false;
    };

    std::unordered_map<int, LaneState> lanes;

    for (const Commit& commit : commits)
    {
        CommitGraphNode node;

        node.commit = commit;
        node.isActive = (activeCommits.find(commit.hash) != activeCommits.end());

        auto laneIt = commitLanes.find(commit.hash);
        int currentLane = (laneIt != commitLanes.end()) ? laneIt->second : 0;
        node.lane = currentLane;

        // Record lanes that existed before processing this commit
        std::unordered_map<int, bool> laneExistedBefore;
        for (const auto& [laneIdx, state] : lanes)
        {
            if (!state.targetHash.empty())
            {
                laneExistedBefore[laneIdx] = true;
            }
        }

        // Find incoming upper line state for this commit in currentLane
        auto currentLaneIt = lanes.find(currentLane);
        if (currentLaneIt != lanes.end() && currentLaneIt->second.targetHash == commit.hash)
        {
            node.hasUpperLine = true;
            node.isUpperLineActive = currentLaneIt->second.isActive;
        }
        else
        {
            node.hasUpperLine = false;
            node.isUpperLineActive = node.isActive;
        }

        // Clear ALL lane entries targeting this commit hash, as this commit has now been reached.
        for (auto& [laneIdx, state] : lanes)
        {
            if (state.targetHash == commit.hash)
            {
                state.targetHash.clear();
                state.isActive = false;
            }
        }

        // Assign parents
        for (std::size_t parentIndex = 0; parentIndex < commit.parents.size(); ++parentIndex)
        {
            const std::string& parent = commit.parents[parentIndex];

            if (parent.empty())
            {
                continue;
            }

            auto pLaneIt = commitLanes.find(parent);
            int parentLane = (pLaneIt != commitLanes.end()) ? pLaneIt->second : currentLane;

            bool parentIsActive = node.isActive && (activeCommits.find(parent) != activeCommits.end());

            bool existingActive = (lanes.count(parentLane) ? lanes[parentLane].isActive : false);
            lanes[parentLane] = {parent, parentIsActive || existingActive};

            if (std::find(node.parentLanes.begin(), node.parentLanes.end(), parentLane) ==
                node.parentLanes.end())
            {
                node.parentLanes.push_back(parentLane);
                node.parentLanesActive.push_back(parentIsActive);
            }
        }

        // -------------------------------------------------
        // Capture lanes that remain active after this row.
        // -------------------------------------------------

        for (const auto& [laneIdx, state] : lanes)
        {
            if (!state.targetHash.empty() && laneIdx != node.lane)
            {
                bool isParentLane = (std::find(node.parentLanes.begin(), node.parentLanes.end(), laneIdx) !=
                                     node.parentLanes.end());
                bool existedBefore = (laneExistedBefore.count(laneIdx) && laneExistedBefore[laneIdx]);

                if (!isParentLane || existedBefore)
                {
                    node.activeLanes.push_back(laneIdx);
                    node.activeLanesActive.push_back(state.isActive);
                }
            }
        }

        // Sort activeLanes by lane index for consistent rendering
        std::vector<std::pair<int, bool>> sortedActive;
        sortedActive.reserve(node.activeLanes.size());
        for (std::size_t i = 0; i < node.activeLanes.size(); ++i)
        {
            sortedActive.push_back({node.activeLanes[i], node.activeLanesActive[i]});
        }
        std::sort(sortedActive.begin(), sortedActive.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        node.activeLanes.clear();
        node.activeLanesActive.clear();
        for (const auto& p : sortedActive)
        {
            node.activeLanes.push_back(p.first);
            node.activeLanesActive.push_back(p.second);
        }

        for (int parentLane : node.parentLanes)
        {
            bool inActive = (std::find(node.activeLanes.begin(), node.activeLanes.end(), parentLane) !=
                             node.activeLanes.end());
            node.parentLanesInActive.push_back(inActive);
        }

        result.push_back(std::move(node));
    }

    return result;
}

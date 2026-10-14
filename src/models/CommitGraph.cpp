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

    // Visual lane tracking structure
    struct LaneState
    {
        std::string targetHash;
        bool isActive = false;
        bool setByChild = false;
    };

    std::vector<LaneState> lanes;

    // Seed lane 0 with active HEAD if present so active branch stays on lane 0
    if (commitMap.find(startHash) != commitMap.end())
    {
        lanes.push_back({startHash, true, false});
    }

    auto findLane = [&](const std::string& hash) -> int {
        for (int i = 0; i < static_cast<int>(lanes.size()); ++i)
        {
            if (lanes[i].targetHash == hash)
            {
                return i;
            }
        }
        return -1;
    };

    auto createLane = [&](const std::string& hash, bool active, bool setByChild) -> int {
        for (int i = 0; i < static_cast<int>(lanes.size()); ++i)
        {
            if (lanes[i].targetHash.empty())
            {
                lanes[i] = {hash, active, setByChild};
                return i;
            }
        }
        lanes.push_back({hash, active, setByChild});
        return static_cast<int>(lanes.size() - 1);
    };

    for (const Commit& commit : commits)
    {
        CommitGraphNode node;

        node.commit = commit;
        node.isActive = (activeCommits.find(commit.hash) != activeCommits.end());

        // Find lane expected for this commit
        int currentLane = findLane(commit.hash);

        if (currentLane >= 0 && lanes[currentLane].setByChild)
        {
            node.hasUpperLine = true;
            node.isUpperLineActive = lanes[currentLane].isActive;
        }
        else
        {
            if (currentLane < 0)
            {
                currentLane = createLane(commit.hash, node.isActive, false);
            }
            node.hasUpperLine = false;
            node.isUpperLineActive = node.isActive;
        }

        node.lane = currentLane;

        // Clear all lane entries expecting this commit (since it's reached now)
        for (auto& l : lanes)
        {
            if (l.targetHash == commit.hash)
            {
                l.targetHash.clear();
                l.isActive = false;
                l.setByChild = false;
            }
        }

        // Assign lanes for commit's parents
        for (std::size_t pIndex = 0; pIndex < commit.parents.size(); ++pIndex)
        {
            const std::string& parentHash = commit.parents[pIndex];

            if (parentHash.empty())
            {
                continue;
            }

            bool parentIsActive = node.isActive && (activeCommits.find(parentHash) != activeCommits.end());

            int pLane = -1;

            if (pIndex == 0)
            {
                // Primary parent ALWAYS continues in currentLane to preserve main branch lineage
                pLane = currentLane;
                bool existingActive = lanes[currentLane].isActive;
                // If another lane was also expecting parentHash, combine active status
                int existingOtherLane = findLane(parentHash);
                if (existingOtherLane >= 0 && existingOtherLane != currentLane)
                {
                    existingActive = existingActive || lanes[existingOtherLane].isActive;
                }
                lanes[currentLane] = {parentHash, parentIsActive || existingActive, true};
            }
            else
            {
                pLane = findLane(parentHash);
                if (pLane >= 0)
                {
                    lanes[pLane].isActive = lanes[pLane].isActive || parentIsActive;
                    lanes[pLane].setByChild = true;
                }
                else
                {
                    pLane = createLane(parentHash, parentIsActive, true);
                }
            }

            if (std::find(node.parentLanes.begin(), node.parentLanes.end(), pLane) == node.parentLanes.end())
            {
                node.parentLanes.push_back(pLane);
                node.parentLanesActive.push_back(parentIsActive);
            }
        }

        // Collect other active lanes running through this row
        for (int i = 0; i < static_cast<int>(lanes.size()); ++i)
        {
            if (i != node.lane && !lanes[i].targetHash.empty())
            {
                node.activeLanes.push_back(i);
                node.activeLanesActive.push_back(lanes[i].isActive);
            }
        }

        result.push_back(std::move(node));
    }

    return result;
}

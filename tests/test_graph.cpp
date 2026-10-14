#include "../src/models/CommitGraph.h"
#include <cassert>
#include <iostream>
#include <vector>

void testBranchCreationInactivity()
{
    std::cout << "Running testBranchCreationInactivity..." << std::endl;

    // Topology:
    // m2 (main tip, active) -> parent m1
    // f1 (feature tip, inactive) -> parent m1
    // m1 (main commit, active) -> parent c0
    // c0 (root, active)

    std::vector<Commit> commits = {
        Commit{"hash_m2", {"hash_m1"}, "Alice", "alice@example.com", "2026-10-14", "Main 2"},
        Commit{"hash_f1", {"hash_m1"}, "Bob", "bob@example.com", "2026-10-14", "Feature 1"},
        Commit{"hash_m1", {"hash_c0"}, "Alice", "alice@example.com", "2026-10-14", "Main 1"},
        Commit{"hash_c0", {}, "Alice", "alice@example.com", "2026-10-14", "Root"}
    };

    // Active HEAD is m2
    auto graph = CommitGraph::build(commits, "hash_m2");

    assert(graph.size() == 4);

    // Row 0: m2 (Main tip)
    assert(graph[0].commit.hash == "hash_m2");
    assert(graph[0].isActive == true);
    assert(graph[0].hasUpperLine == false);
    assert(graph[0].isUpperLineActive == true);
    assert(graph[0].parentLanes.size() == 1);
    assert(graph[0].parentLanesActive[0] == true); // Main branch line active

    // Row 1: f1 (Feature tip)
    assert(graph[1].commit.hash == "hash_f1");
    assert(graph[1].isActive == false); // Feature branch is inactive!
    assert(graph[1].hasUpperLine == false);
    // f1 parent is m1. Even though m1 is active, f1 -> m1 edge is INACTIVE!
    assert(graph[1].parentLanes.size() == 1);
    assert(graph[1].parentLanesActive[0] == false); // INACTIVE!
    // Main lane (hash_m1) is passing through f1 row
    assert(graph[1].activeLanes.size() == 1);
    assert(graph[1].activeLanesActive[0] == true); // Main lane stays active

    // Row 2: m1 (Main 1)
    assert(graph[2].commit.hash == "hash_m1");
    assert(graph[2].isActive == true);
    assert(graph[2].hasUpperLine == true);
    assert(graph[2].isUpperLineActive == true); // Coming from m2 (active)
    assert(graph[2].parentLanes.size() == 1);
    assert(graph[2].parentLanesActive[0] == true); // m1 -> c0 is active

    // Row 3: c0 (Root)
    assert(graph[3].commit.hash == "hash_c0");
    assert(graph[3].isActive == true);
    assert(graph[3].hasUpperLine == true);
    assert(graph[3].isUpperLineActive == true);

    std::cout << "  -> PASSED!" << std::endl;
}

void testBranchCreatedFromMergeCommit()
{
    std::cout << "Running testBranchCreatedFromMergeCommit..." << std::endl;

    // Topology:
    // m3 (main tip, active) -> parent m2
    // m2 (main merge, active) -> parents m1, f_merged
    // f_new (unmerged branch from m2, inactive) -> parent m2
    // f_merged (merged branch, active) -> parent m1
    // m1 (active) -> parent c0
    // c0 (root)

    std::vector<Commit> commits = {
        Commit{"hash_m3", {"hash_m2"}, "Alice", "alice@example.com", "2026-10-14", "Main 3"},
        Commit{"hash_f_new", {"hash_m2"}, "Charlie", "charlie@example.com", "2026-10-14", "New Feature"},
        Commit{"hash_m2", {"hash_m1", "hash_f_merged"}, "Alice", "alice@example.com", "2026-10-14", "Merge"},
        Commit{"hash_f_merged", {"hash_m1"}, "Bob", "bob@example.com", "2026-10-14", "Merged Feature"},
        Commit{"hash_m1", {"hash_c0"}, "Alice", "alice@example.com", "2026-10-14", "Main 1"},
        Commit{"hash_c0", {}, "Alice", "alice@example.com", "2026-10-14", "Root"}
    };

    auto graph = CommitGraph::build(commits, "hash_m3");

    assert(graph.size() == 6);

    // Check f_new (row 1)
    assert(graph[1].commit.hash == "hash_f_new");
    assert(graph[1].isActive == false);
    assert(graph[1].parentLanes.size() == 1);
    assert(graph[1].parentLanesActive[0] == false); // Edge f_new -> m2 must be INACTIVE!

    // Check passing activeLanes in f_new row
    assert(graph[1].activeLanesActive[0] == true); // main lane hash_m2 passing through f_new is ACTIVE

    // Check m2 (row 2)
    assert(graph[2].commit.hash == "hash_m2");
    assert(graph[2].isActive == true);
    assert(graph[2].isUpperLineActive == true); // Main upper line coming into m2 is active

    std::cout << "  -> PASSED!" << std::endl;
}

int main()
{
    testBranchCreationInactivity();
    testBranchCreatedFromMergeCommit();

    std::cout << "ALL GRAPH TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}

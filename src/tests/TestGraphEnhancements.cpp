#include "../models/CommitGraph.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

void testTopCommitUpwardLineFix()
{
    std::cout << "[Test] Top commit upward line fix..." << std::endl;

    std::vector<Commit> commits;

    Commit c2;
    c2.hash = "hash_c2";
    c2.parents = {"hash_c1"};
    c2.subject = "Commit 2";

    Commit c1;
    c1.hash = "hash_c1";
    c1.parents = {};
    c1.subject = "Commit 1";

    commits = {c2, c1};

    auto graph = CommitGraph::build(commits, "hash_c2");

    assert(graph.size() == 2);

    // c2 is top commit in lane 0
    assert(graph[0].commit.hash == "hash_c2");
    assert(graph[0].hasUpperLine == false); // Must NOT draw upward line for top commit!
    assert(graph[0].isActive == true);

    // c1 is second commit in lane 0
    assert(graph[1].commit.hash == "hash_c1");
    assert(graph[1].hasUpperLine == true); // MUST draw incoming line from c2!
    assert(graph[1].isActive == true);

    std::cout << "  -> PASSED!" << std::endl;
}

void testBranchLineColoringActiveVsInactive()
{
    std::cout << "[Test] Branch line coloring active vs inactive..." << std::endl;

    // Structure:
    // f1 (unmerged feature commit) -> f0 (unmerged feature commit) -> c1
    // m1 (main branch tip, HEAD) -> c1
    // c1 (shared root commit)

    Commit f1;
    f1.hash = "hash_f1";
    f1.parents = {"hash_f0"};
    f1.subject = "Feature commit 1";

    Commit m1;
    m1.hash = "hash_m1";
    m1.parents = {"hash_c1"};
    m1.subject = "Main commit 1";

    Commit f0;
    f0.hash = "hash_f0";
    f0.parents = {"hash_c1"};
    f0.subject = "Feature commit 0";

    Commit c1;
    c1.hash = "hash_c1";
    c1.parents = {};
    c1.subject = "Root commit 1";

    // Reverse chronological order: f1, m1, f0, c1
    std::vector<Commit> commits = {f1, m1, f0, c1};

    // Build graph with active branch HEAD = m1
    auto graph = CommitGraph::build(commits, "hash_m1");

    assert(graph.size() == 4);

    // Row 0: f1 (unmerged side branch tip)
    assert(graph[0].commit.hash == "hash_f1");
    assert(graph[0].hasUpperLine == false);
    assert(graph[0].isActive == false); // Grayed out!
    assert(graph[0].parentLanesActive.size() == 1);
    assert(graph[0].parentLanesActive[0] == false); // Parent f0 is also unmerged!

    // Row 1: m1 (active branch tip)
    assert(graph[1].commit.hash == "hash_m1");
    assert(graph[1].hasUpperLine == false);
    assert(graph[1].isActive == true); // Active branch highlighted!
    assert(graph[1].parentLanesActive.size() == 1);
    assert(graph[1].parentLanesActive[0] == true); // Main line active

    // Check activeLanes for m1 (side branch holding f0 passing through/active)
    assert(graph[1].activeLanes.size() == 1);
    assert(graph[1].activeLanesActive.size() == 1);
    assert(graph[1].activeLanesActive[0] == false); // Side branch line segment grayed out!

    // Row 2: f0 (unmerged feature commit 0)
    assert(graph[2].commit.hash == "hash_f0");
    assert(graph[2].hasUpperLine == true);
    assert(graph[2].isActive == false); // Unmerged!

    // Row 3: c1 (shared root commit)
    assert(graph[3].commit.hash == "hash_c1");
    assert(graph[3].hasUpperLine == true);
    assert(graph[3].isActive == true); // Active!

    std::cout << "  -> PASSED!" << std::endl;
}

int main()
{
    testTopCommitUpwardLineFix();
    testBranchLineColoringActiveVsInactive();

    std::cout << "ALL GRAPH ENHANCEMENT TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}

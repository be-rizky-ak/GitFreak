#pragma once

#include "MainWindow.h"

#include "../models/Branch.h"
#include "../models/Commit.h"
#include "../models/CommitGraph.h"
#include "../models/Diff.h"
#include "../models/OperationLog.h"
#include "../models/OperationState.h"
#include "../models/RepositoryStatus.h"

#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class GitRepository;
class TaskScheduler;

class AppController
{
public:
    AppController(
        const slint::ComponentHandle<MainWindow>& window,
        TaskScheduler& scheduler);

    ~AppController();

    AppController(const AppController&) = delete;
    AppController& operator=(const AppController&) = delete;

private:
    void connectSignals();
    void openRepository();
    void pollRepositoryOpen();
    void pollStatus();
    void stageFile(int index);
    void unstageFile(int index);
    void startFileOperation(
        int index,
        bool stage);
    void pollFileOperation();
    void startStatusRefresh();
    void commit(
        const std::string& message);
    void pollCommit();
    void startHistoryRefresh();
    void pollHistory();
    void selectFile(int index);
    void startDiffRefresh(
        const std::filesystem::path& path,
        bool staged);
    void pollDiff();
    void startBranchRefresh();
    void pollBranches();

    void checkoutBranch(int index);
    void startCheckout(
        const std::string& branchName);
    void pollCheckout();

    void fetch();
    void startFetch();
    void pollFetch();

    void pull();
    void startPull();
    void pollPull();

    void push();
    void startPush();
    void pollPush();

    void cloneRepository(
        const std::string& url,
        const std::filesystem::path& destination);
    void startClone(
        const std::string& url,
        const std::filesystem::path& destination);
    void pollClone();

    void updateHistoryGraph(
        const std::vector<Commit>& commits);

    void startOperation(
        const std::string& title,
        const std::string& command);
    void appendOperationLog(
        bool isError,
        const std::string& text);
    void finishOperation(
        bool success,
        int exitCode);
    void pollOperationLog();
    void closeOperationDialog();

    void showCloneDialog();
    void browseCloneDestination();

    slint::ComponentHandle<MainWindow> m_window;

    TaskScheduler& m_scheduler;

    std::unique_ptr<GitRepository> m_repository;

    std::future<std::optional<std::filesystem::path>>
        m_openRepositoryTask;

    std::future<RepositoryStatus> m_statusTask;
    RepositoryStatus m_repositoryStatus;
    std::future<void> m_fileOperationTask;
    std::vector<std::filesystem::path> m_fileOrder;
    std::future<void> m_commitTask;
    std::future<std::vector<Commit>> m_historyTask;
    std::future<Diff> m_diffTask;
    std::future<std::vector<Branch>> m_branchTask;
    std::future<void> m_checkoutTask;
    std::vector<Branch> m_branches;
    std::future<int> m_fetchTask;
    std::future<int> m_pullTask;
    std::future<int> m_pushTask;
    std::future<int> m_cloneTask;

    std::shared_ptr<OperationLog> m_operationLog;
    OperationState m_operationState;

    slint::Timer m_repositoryTimer;
    slint::Timer m_operationTimer;
    std::vector<std::string> m_operationLines;
    std::string m_operationPendingText;
    std::filesystem::path m_cloneDestination;
};
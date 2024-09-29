#pragma once

#include <atomic>
#include <deque>
#include <mutex>

#include "EngineInterface/SerializerService.h"

#include "Definitions.h"
#include "PersisterJob.h"

class PersisterWorker
{
public:
    PersisterWorker(SimulationController const& simController);

    void runThreadLoop();
    void shutdown();

    void saveToDisc(std::string const& filename);

private:
    void processJobs(std::unique_lock<std::mutex>& lock);

    PersisterJobResult processSaveToDiscJob(std::unique_lock<std::mutex>& lock, SaveToDiscJob const& job);

    SimulationController _simController;

    std::atomic<bool> _isShutdown{false};

    std::mutex _jobMutex;
    std::deque<PersisterJob> _openJobs;
    std::vector<PersisterJob> _inProgressJobs;
    std::vector<PersisterJobResult> _finishedJobs;

    int _idCount = 0;
    std::condition_variable _conditionVariable;
};

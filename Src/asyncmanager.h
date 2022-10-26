#pragma once
#include <queue>
#include <condition_variable>
#include <thread>
#include <functional>

class AsyncManager final
{

public:
    AsyncManager(size_t numberOfThreads);
    ~AsyncManager();

    bool StartAsyncRequest(const std::function<void(void)>& function);
    void StopThreads();
    bool IsRunning();

private:

    void WorkerThread();

    // need to keep track of threads so we can join them
    std::vector< std::thread > m_workers;

    //queue of tasks to be consumed by threads
    std::queue< std::function<void(void)> > m_tasks;

    // synchronization
    std::mutex				m_queue_mutex;
    std::condition_variable_any m_condition;
    bool m_stop;
};

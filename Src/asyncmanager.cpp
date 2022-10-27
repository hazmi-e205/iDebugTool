#include "asyncmanager.h"
#include <QDebug>

AsyncManager *AsyncManager::m_instance = nullptr;
AsyncManager *AsyncManager::Get()
{
    if(!m_instance)
        m_instance = new AsyncManager();
    return m_instance;
}

void AsyncManager::Destroy()
{
    if (m_instance)
    {
        delete m_instance;
        m_instance = nullptr;
    }
}

void AsyncManager::WorkerThread()
{
    for (;;)
    {
        std::function<void(void)> myFunc;
        {
            //LOG_INFO("%s | waiting for notification!", __FUNCTION__);
            std::unique_lock<std::mutex> lock(this->m_queue_mutex);
            m_condition.wait(lock, [this]
            {
                return m_stop || !m_tasks.empty();
            }
            );

            if (m_stop && m_tasks.empty())
            {
                return;
            }

            myFunc = std::move(m_tasks.front());
            m_tasks.pop();
            //LOG_INFO("%s | running task", __FUNCTION__);
        }

        if (myFunc)
        {
            myFunc();
        }
        else
        {
            //LOG_ERROR( "%s | ERROR, empty function pointer for task", __FUNCTION__);
        }
    }
}

AsyncManager::AsyncManager()
{
}

AsyncManager::~AsyncManager()
{
    StopThreads();
}

void AsyncManager::Init(size_t numberOfThreads)
{
    m_stop = false;
    for (size_t i = 0; i < numberOfThreads; ++i)
    {
        std::thread t(&AsyncManager::WorkerThread, this);
        m_workers.emplace_back(std::move(t));
    }
}

bool AsyncManager::StartAsyncRequest(const std::function<void(void)>& function)
{
    {
        std::unique_lock<std::mutex> lock(m_queue_mutex);
        m_tasks.emplace(function);
    }

    m_condition.notify_one();
    return true;
}

void AsyncManager::StopThreads()
{
    //ms todo WHAT do we do if we need to stop the threads ? do we still execute pending tasks ?
    // discard them ?
    {
        std::unique_lock<std::mutex> lock(m_queue_mutex);
        m_stop = true;
    }

    m_condition.notify_all();

    for (std::thread &worker : m_workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
        else
        {
            //LOG_WARNING("%s | could not stop running task", __FUNCTION__);
        }
    }
}
bool AsyncManager::IsRunning()
{
    return !m_tasks.empty();
}

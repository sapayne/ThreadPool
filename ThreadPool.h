#pragma once
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <atomic>
#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class Threadpool {

public:
    Threadpool(unsigned int);
    bool working()
    {
        return tasks.size();
    }
    int numOfThreads()
    {
        return workers.size();
    }
    int changeNumOfThreads(unsigned int Amount);
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)->std::future<decltype(f(args...))>;
    ~Threadpool();
private:
    std::string output;
    // need to keep track of threads so we can join them
    std::vector<std::thread> workers;
    // the task queue
    std::queue<std::function<void()>> tasks;

    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic_bool stop;
    //  used when removing threads from the threadpool
    std::vector<bool> preciseThreadStop;
    void addThreads(int amount);
};

// the constructor creates as many threads as possible if a value isn't passed
inline Threadpool::Threadpool(unsigned int threads = std::thread::hardware_concurrency()) : stop(false)
{
    threads = std::min(std::max(threads, (unsigned)1), std::thread::hardware_concurrency());
    addThreads(threads);
}

// add new work item to the pool
template<class F, class... Args>
auto Threadpool::enqueue(F&& f, Args&&... args) -> std::future<decltype(f(args...))>
{
    using return_type = decltype(f(args...));

    auto task = std::make_shared< std::packaged_task<return_type()> >(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if (stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace([task]() { (*task)(); });
    }
    condition.notify_one();
    return res;
}

inline void Threadpool::addThreads(int amount)
{
    unsigned int startingIndex = workers.size(), endingIndex = startingIndex + amount;
    for (unsigned int i = startingIndex; i < endingIndex; i++)
    {
        preciseThreadStop.emplace_back(false);
        workers.emplace_back(
            [this, i]
            {
                while (true)
                {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock,
                            [this, i] {
                                return this->stop || !this->tasks.empty() || this->preciseThreadStop[i];
                            });
                        if ((this->stop && this->tasks.empty()) || this->preciseThreadStop[i]) return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                }
            }
        );
    }
}

inline int Threadpool::changeNumOfThreads(unsigned int Amount)
{
    /*  keeps the Threadpool from allocating more threads than what the cpu can handle, so that the program 
        doesn't have to suffer from context switching given it is the only program running, and from having less than 1 thread.
    */
    int diff = std::min(std::max(Amount, (unsigned)1), std::thread::hardware_concurrency()) - workers.size();
    std::cout << "Changing number of threads from " << workers.size() << " to " << Amount << std::endl;

    if (diff > 0)
    {
        addThreads(diff);
    }
    else if (diff < 0)
    {
        int endingIndex = abs(diff);
        //  removes the first positive diff number of threads starting at index 0
        for (int i = 0; i < endingIndex; i++)
        {
            preciseThreadStop[i] = true;
            /*  call notify_all multiple times or have another loop to join and delete the workers of the corresponding indices, 
                not sure which impacts the program running time more
            */
            condition.notify_all();
            workers[i].join();
            workers[i].~thread();
        }
        //  deallocate the old memory, as the joined threads can no longer except any tasks after joining.
        workers.erase(workers.begin(), workers.begin() + endingIndex);
        preciseThreadStop.erase(preciseThreadStop.begin(), preciseThreadStop.begin() + endingIndex);
    }
    std::cout << "Number of threads after change: " << workers.size() << std::endl;
    return workers.size();
}

// the destructor joins all threads
inline Threadpool::~Threadpool()
{
    //std::unique_lock<std::mutex> lock(queue_mutex);
    stop = true;
    condition.notify_all();
    for (std::thread& worker : workers) {
        worker.join();
        worker.~thread();
    }
    workers.~vector();
    preciseThreadStop.~vector();
}

#endif
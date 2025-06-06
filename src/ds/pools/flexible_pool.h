#ifndef FLEXIBLE_POOL_H
#define FLEXIBLE_POOL_H

#include <vector>
#include <thread>
#include <functional>
#include <iostream>
#include <mutex>
#include <atomic>
#include <chrono>
// #include <cassert>

#include <vector>
#include <thread>
#include <functional>
#include <iostream>
#include <mutex>
#include <atomic>
#include <chrono>
// #include <cassert>

// FLEXIBLEPOOL IS NOT THREAD-SAFE! (ONLY ONE THREAD SUPPOSED TO HAVE ACCESS TO THE THREAD POOL) 
template<template<class> class QueueType>
class FlexiblePool {
public:
    enum class ControlSignal { OK, RESET, STOP };


    using TaskType = std::function<ControlSignal()>;
    
    explicit FlexiblePool(unsigned int num_workers);
    explicit FlexiblePool(unsigned int num_workers, QueueType<TaskType> &&tasks);
    ~FlexiblePool();

    void start();
    
    template <class F, class... Args>
    void push(F&& f, Args&&... args);
    void stop();
    void reset();

    size_t get_active_workers() const {
        return num_active_workers;
    }

private:
    unsigned int num_workers;
    std::vector<std::thread> workers;
    QueueType<TaskType> tasks;
    std::thread::id owner;

    bool stopped = false;
    std::atomic<bool> running{false};

    std::atomic<size_t> num_active_workers{0};

    void do_work();

};

template<template<class> class QueueType>
void FlexiblePool<QueueType>::start() {
    running = true;
    running.notify_all();
}

template<template<class> class QueueType>
void FlexiblePool<QueueType>::reset() {
    if (std::this_thread::get_id() != owner) {
        std::cerr << "[FlexiblePool] [ERROR] Non-owning thread attempted to reset the thread pool.\n";
        return;
    }

    // Need to ensure there are active workers first
    size_t cur_active_workers;
    while ((cur_active_workers = num_active_workers) < num_workers) {
        num_active_workers.wait(cur_active_workers);
    }

    running = false;
    running.notify_all();
    
    stopped = false;

    for (size_t i = 0; i < num_workers; ++i) {
        tasks.push([]() -> FlexiblePool::ControlSignal {
            return FlexiblePool::ControlSignal::RESET;
        });
    }
    
    // Wait for all workers to acknowledge reset
    while ((cur_active_workers = num_active_workers) > 0) {
        num_active_workers.wait(cur_active_workers);
    }

    // assert(tasks.empty());
}

template<template<class> class QueueType>
void FlexiblePool<QueueType>::do_work() {
    // Signal that this worker has reset and is now waiting
    while (true) {
        running.wait(false);
        num_active_workers.fetch_add(1);
        num_active_workers.notify_all();

        while (true) {
            TaskType task;
            if (tasks.pop(task)) {
                ControlSignal signal = task();
                if (signal == ControlSignal::STOP) {
                    num_active_workers.fetch_sub(1);
                    num_active_workers.notify_all();
                    return;
                }
                else if (signal == ControlSignal::RESET) {
                    num_active_workers.fetch_sub(1);
                    num_active_workers.notify_all();
                    break;
                }
            }
            else if (!tasks.is_blocking()) { // known at compile-time as is_blocking is constexpr function
                std::this_thread::yield();
            }
            else {
                std::cerr << "[FlexiblePool] [FATAL] Blocking queue returns false from pop().\n";
                return;
            }
        }
    }
}

template<template<class> class QueueType>
FlexiblePool<QueueType>::FlexiblePool(unsigned int num_workers) : num_workers(num_workers), owner(std::this_thread::get_id()) {
    workers.resize(num_workers);
    for (unsigned int i = 0; i < num_workers; ++i) {
        workers[i] = std::thread(&FlexiblePool::do_work, this);
    }
}

template<template<class> class QueueType>
FlexiblePool<QueueType>::FlexiblePool(unsigned int num_workers, QueueType<TaskType> &&tasks) : num_workers(num_workers), tasks(std::move(tasks)), owner(std::this_thread::get_id()) {
    workers.resize(num_workers);
    for (unsigned int i = 0; i < num_workers; ++i) {
        workers[i] = std::thread(&FlexiblePool::do_work, this);
    }
}

template<template<class> class QueueType>
FlexiblePool<QueueType>::~FlexiblePool() {
    if (!stopped) {
        stop();
    }
}

template<template<class> class QueueType>
template<class F, class... Args>
void FlexiblePool<QueueType>::push(F&& f, Args&&... args) {
    tasks.push([f = std::forward<F>(f), 
                args_tuple = std::tuple<std::decay_t<Args>...>(std::forward<Args>(args)...)]
               () mutable noexcept -> FlexiblePool::ControlSignal {
        std::apply(std::move(f), std::move(args_tuple));
        return FlexiblePool::ControlSignal::OK;
    });
}


// can only be called by owning thread
template<template<class> class QueueType>
void FlexiblePool<QueueType>::stop() {    
    if (std::this_thread::get_id() != owner) {
        std::cerr << "[FlexiblePool] [ERROR] Non-owning thread attempted to stop the thread pool.\n";
        return;
    }
    if (!running) {
        running = true;
        running.notify_all();
    }
    for (size_t i = 0; i < num_workers; ++i) {
        tasks.push([]() -> FlexiblePool::ControlSignal {
            return FlexiblePool::ControlSignal::STOP; 
        });
    }
    for (size_t i = 0; i < num_workers; ++i) {
        if (workers[i].joinable()) {
            workers[i].join();
        }
    }
    stopped = true;
}

#endif
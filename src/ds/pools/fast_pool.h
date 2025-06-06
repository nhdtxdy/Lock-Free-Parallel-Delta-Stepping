#ifndef FAST_POOL_H
#define FAST_POOL_H

#include <vector>
#include <thread>
#include <functional>
#include <barrier>
#include <type_traits>
// #include <cassert>

// FASTPOOL IS NOT THREAD-SAFE! (ONLY ONE THREAD SUPPOSED TO HAVE ACCESS TO THE THREAD POOL) 
template<template<class> class QueueType>
class FastPool {
public:
    enum class ControlSignal { OK, STOP };
    using TaskType = std::function<ControlSignal()>;
    
    explicit FastPool(unsigned int num_workers);
    ~FastPool();

    void start();
    
    template <class F, class... Args>
    void push(F&& f, Args&&... args);
    void stop();
    void reset();

private:
    size_t num_workers;
    std::vector<std::thread> workers;
    QueueType<TaskType> tasks;
    std::thread::id owner;

    bool stopped = false;
    
    // Members only present for non-blocking queues
    std::atomic<size_t> num_active_workers{0};
    std::atomic<bool> running{false};

    std::barrier<> barrier;

    void do_work();

};

template<template<class> class QueueType>
FastPool<QueueType>::FastPool(unsigned int num_workers) 
    : num_workers(num_workers),
      owner(std::this_thread::get_id()),
      barrier(num_workers + 1) {
    
    workers.resize(num_workers);
    for (size_t i = 0; i < num_workers; ++i) {
        workers[i] = std::thread(&FastPool::do_work, this);
    }
}

template<template<class> class QueueType>
FastPool<QueueType>::~FastPool() {
    if (!stopped) {
        stop();
    }
}

template<template<class> class QueueType>
void FastPool<QueueType>::start() {
    // For blocking queues, workers are always running
}

template<template<class> class QueueType>
void FastPool<QueueType>::reset() {
    for (size_t i = 0; i < num_workers; ++i) {
        tasks.push([this]() -> FastPool::ControlSignal {
            barrier.arrive_and_wait();
            return FastPool::ControlSignal::OK;
        });
    }
    barrier.arrive_and_wait();
}

template<template<class> class QueueType>
void FastPool<QueueType>::do_work() {
    while (true) {
        TaskType task;
        if (tasks.pop(task)) {
            ControlSignal signal = task();
            if (signal == ControlSignal::STOP) {
                return;
            }
        }
    }
}

template<template<class> class QueueType>
template<class F, class... Args>
void FastPool<QueueType>::push(F&& f, Args&&... args) {
    tasks.push([f = std::forward<F>(f), 
                args_tuple = std::tuple<std::decay_t<Args>...>(std::forward<Args>(args)...)]
               () mutable noexcept -> FastPool::ControlSignal {
        std::apply(std::move(f), std::move(args_tuple));
        return FastPool::ControlSignal::OK;
    });
}


// can only be called by owning thread
template<template<class> class QueueType>
void FastPool<QueueType>::stop() {    
    for (size_t i = 0; i < num_workers; ++i) {
        tasks.push([]() -> FastPool::ControlSignal {
            return FastPool::ControlSignal::STOP; 
        });
    }
    for (size_t i = 0; i < num_workers; ++i) {
        workers[i].join();
    }
    stopped = true;
}

#endif
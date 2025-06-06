#ifndef SIMPLE_POOL_H
#define SIMPLE_POOL_H

#include <chrono>

template<template<class> class QueueType>
class SimplePool {
        unsigned int num_workers;
        std::vector<std::thread> workers;
        QueueType<std::function<bool()>> tasks;
        bool stopped = false;

        void do_work();
        // create workers: workers[i] = std::thread(&do_work)
    public:
        using TaskType = std::function<bool()>;
        SimplePool(unsigned int num_workers = 0);
        ~SimplePool();
        template <class F, class... Args>
        void push(F f, Args ... args);
        void stop();
};

template<template<class> class QueueType>
void SimplePool<QueueType>::do_work() {
    while (true) {
        // note: using while (true) is fine as SafeUnboundedQueue::pop() blocks until the queue becomes non-empty
        TaskType task;
        tasks.pop(task);
        if (!task()) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

template<template<class> class QueueType>
SimplePool<QueueType>::SimplePool(unsigned int num_workers): num_workers(num_workers) {
    workers.resize(num_workers);
    for (unsigned int i = 0; i < num_workers; ++i) {
        workers[i] = std::thread(&SimplePool::do_work, this);
    }
}

template<template<class> class QueueType>
SimplePool<QueueType>::~SimplePool() {
    if (!stopped) {
        stop();
    }
}

template<template<class> class QueueType>
template <class F, class... Args>
void SimplePool<QueueType>::push(F f, Args ... args) {
    // note: cannot capture by reference (possible dangling reference when push returns?)
    tasks.push([=]() -> bool {
        f(args...);
        return true;
    });
}

template<template<class> class QueueType>
void SimplePool<QueueType>::stop() {
    for (unsigned int i = 0; i < num_workers; ++i) {
        tasks.push([]() -> bool {
            return false;
        });
    }
    for (unsigned int i = 0; i < num_workers; ++i) {
        workers[i].join();
    }
    stopped = true;
}

#endif
#ifndef SIMPLE_POOL_H
#define SIMPLE_POOL_H

#include <vector>
#include <thread>
#include <functional>
#include <iostream>

template<template<class> class QueueType>
class SimplePool {
public:
    using TaskType = std::function<bool()>;
    
    explicit SimplePool(unsigned int num_workers);
    explicit SimplePool(unsigned int num_workers, QueueType<TaskType> &&tasks);
    ~SimplePool();
    
    template <class F, class... Args>
    void push(F&& f, Args&&... args);
    
    void stop();

private:
    unsigned int num_workers;
    std::vector<std::thread> workers;
    QueueType<TaskType> tasks;

    bool stopped = false;

    void do_work();
};

template<template<class> class QueueType>
void SimplePool<QueueType>::do_work() {
    while (true) {
        TaskType task;
        if (tasks.pop(task)) {
            if (!task()) {
                return;
            }
        }
        else if (!tasks.is_blocking()) { // known at compile-time as is_blocking is constexpr function
            std::this_thread::yield();
        }
        else {
            std::cerr << "[SimplePool] [FATAL] Blocking queue returns nothing from pop().\n";
            exit(1);
        }
    }
}

template<template<class> class QueueType>
SimplePool<QueueType>::SimplePool(unsigned int num_workers) : num_workers(num_workers) {
    workers.reserve(num_workers);
    for (unsigned int i = 0; i < num_workers; ++i) {
        workers.emplace_back(&SimplePool::do_work, this);
    }
}

template<template<class> class QueueType>
SimplePool<QueueType>::SimplePool(unsigned int num_workers, QueueType<TaskType> &&tasks) : num_workers(num_workers), tasks(std::move(tasks)) {
    workers.reserve(num_workers);
    for (unsigned int i = 0; i < num_workers; ++i) {
        workers.emplace_back(&SimplePool::do_work, this);
    }
}

template<template<class> class QueueType>
SimplePool<QueueType>::~SimplePool() {
    if (!stopped) {
        stop();
    }
}

template<template<class> class QueueType>
template<class F, class... Args>
void SimplePool<QueueType>::push(F&& f, Args&&... args) {
    tasks.push([f = std::forward<F>(f), args...]() -> bool {
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
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    stopped = true;
}

#endif
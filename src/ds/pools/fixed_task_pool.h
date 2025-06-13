#ifndef FIXED_TASK_POOL_H
#define FIXED_TASK_POOL_H

#include <vector>
#include <thread>
#include <functional>
#include <type_traits>
#include <atomic>
#include <iostream>
#include <barrier>
// #include <cassert>

// FASTPOOL IS NOT THREAD-SAFE! (ONLY ONE THREAD SUPPOSED TO HAVE ACCESS TO THE THREAD POOL) 
class FixedTaskPool {
public:
    enum class ControlSignal { OK, STOP };
    using TaskType = std::function<bool()>;
    
    explicit FixedTaskPool(size_t num_workers, std::barrier<> &barrier): num_workers(num_workers), tasks(num_workers), ready(num_workers) {
        for (size_t i = 0; i < num_workers; ++i) {
            ready[i].store(false);
            workers.emplace_back([this, i, &barrier] {
                while (true) {
                    ready[i].wait(false);
                    if (!tasks[i]()) {
                        return;
                    }
                    ready[i].store(false);
                    barrier.arrive_and_wait();
                }
            });
        }
    }

    ~FixedTaskPool() {
        if (!stopped) {
            for (size_t i = 0; i < num_workers; ++i) {
                tasks[i] = ([] {
                    return false;
                });
                ready[i].store(true);
                ready[i].notify_one();
            }
            for (size_t i = 0; i < num_workers; ++i) {
                workers[i].join();
            }
        }
    }
    
    template <class F, class... Args>
    void push(size_t tid, F&& f, Args&&... args) {
        tasks[tid] = ([f = std::forward<F>(f), 
                    args_tuple = std::tuple<std::decay_t<Args>...>(std::forward<Args>(args)...)]
                    () noexcept {
            std::apply(std::move(f), std::move(args_tuple));
            return true;
        });
        ready[tid].store(true);
        ready[tid].notify_one();
    }

    void stop() {
        for (size_t i = 0; i < num_workers; ++i) {
            tasks[i] = ([] {
                return false;
            });
            ready[i].store(true);
            ready[i].notify_one();
        }
        for (size_t i = 0; i < num_workers; ++i) {
            workers[i].join();
        }
        stopped = true;
    }

private:
    size_t num_workers;
    std::vector<std::thread> workers;
    std::vector<TaskType> tasks;
    std::vector<std::atomic<bool>> ready;
    bool stopped = false;
};


#endif

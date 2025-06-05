#ifndef COARSE_GRAINED_UNBOUNDED_QUEUE_H
#define COARSE_GRAINED_UNBOUNDED_QUEUE_H

#include "thread_safe_queue_base.h"
#include <condition_variable>
#include <mutex>
#include <queue>

template <class E> 
class CoarseGrainedUnboundedQueue : public ThreadSafeQueueBase<E> {
public: 
    CoarseGrainedUnboundedQueue() {}

    // Move semantics
    CoarseGrainedUnboundedQueue(CoarseGrainedUnboundedQueue &&other) noexcept;
    CoarseGrainedUnboundedQueue& operator=(CoarseGrainedUnboundedQueue &&other) noexcept;

    void push(const E& element) override;
    bool pop (E &res) override;
    bool empty() const override {
        return elements.empty();
    }
    constexpr bool is_blocking() const override {
        return true;
    }
    constexpr bool is_lock_free() const override {
        return false;
    }
private:
    std::queue<E> elements;
    std::mutex lock;
    std::condition_variable not_empty;
};

template <class E>
void CoarseGrainedUnboundedQueue<E>::push(const E& element) {
    std::lock_guard<std::mutex> lk(lock);
    elements.push(element);
    not_empty.notify_one();
    return;
}

template <class E> 
bool CoarseGrainedUnboundedQueue<E>::pop(E &res) {
    std::unique_lock<std::mutex> lk(lock);
    while (elements.empty()) {
        not_empty.wait(lk);
    }
    res = std::move(elements.front());
    elements.pop();
    return true;
}

template <class E>
CoarseGrainedUnboundedQueue<E>::CoarseGrainedUnboundedQueue(CoarseGrainedUnboundedQueue &&other) noexcept {
    std::lock_guard<std::mutex> lk(other.lock);
    elements = std::move(other.elements);
}

template <class E>
CoarseGrainedUnboundedQueue<E>& CoarseGrainedUnboundedQueue<E>::operator=(CoarseGrainedUnboundedQueue &&other) noexcept {
    if (this != &other) {
        std::scoped_lock<std::mutex> lk(lock, other.lock);
        elements = std::move(other.elements);
    }
    return *this;
}


#endif
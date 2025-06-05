#ifndef THREAD_SAFE_QUEUE_BASE_H
#define THREAD_SAFE_QUEUE_BASE_H

template <class E>
class ThreadSafeQueueBase {
public:
    virtual ~ThreadSafeQueueBase() = default;
    virtual void push(const E &element) = 0;
    bool pop(E &res) = 0;
    bool empty() const = 0;
    constexpr bool is_blocking() const = 0;
};

#endif
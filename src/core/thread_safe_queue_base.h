#ifndef THREAD_SAFE_QUEUE_BASE_H
#define THREAD_SAFE_QUEUE_BASE_H

template <class E>
class ThreadSafeQueueBase {
public:
    virtual ~ThreadSafeQueueBase() = default;

    virtual void push(const E &element) = 0;
    virtual void push(E &&element) = 0;
    virtual bool pop(E &res) = 0;
    virtual bool empty() const = 0;
    // static constexpr bool is_blocking();
    // static constexpr bool is_lock_free();
};

#endif
#ifndef THREAD_SAFE_QUEUE_BASE_H
#define THREAD_SAFE_QUEUE_BASE_H

template <class E>
class ThreadSafeQueueBase {
protected:
    ThreadSafeQueueBase() = default;
public:
    void push(const E &element);
    bool pop(E &res);
    bool empty() const;
    constexpr bool is_blocking() const;
};

#endif
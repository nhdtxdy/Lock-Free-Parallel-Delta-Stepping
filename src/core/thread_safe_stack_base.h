#ifndef THREAD_SAFE_STACK_BASE_H
#define THREAD_SAFE_STACK_BASE_H

template<class E>
class ThreadSafeStackBase {
public:
    virtual ~ThreadSafeStackBase() = default;

    virtual void push(const E &element) = 0;
    virtual void push(E &&element) = 0;
    virtual bool pop(E &res) = 0;
    virtual bool empty() const = 0;
    virtual constexpr bool is_blocking() const = 0;
    virtual constexpr bool is_lock_free() const = 0;
};

#endif
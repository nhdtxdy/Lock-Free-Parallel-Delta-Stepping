#ifndef THREAD_SAFE_LIST_BASE_H
#define THREAD_SAFE_LIST_BASE_H

#include <vector>

// Base class for doubly linked list

template<class E>
class ThreadSafeListBase {
public:
    virtual ~ThreadSafeListBase() = default;

    virtual std::vector<E> list_all_and_clear() = 0;
    virtual constexpr bool is_blocking() const = 0;
    virtual constexpr bool is_lock_free() const = 0;
};

#endif
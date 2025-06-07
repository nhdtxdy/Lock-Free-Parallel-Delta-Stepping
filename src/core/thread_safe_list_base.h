#ifndef THREAD_SAFE_LIST_BASE_H
#define THREAD_SAFE_LIST_BASE_H

#include <vector>

// Base class for doubly linked list

template<class E>
class ThreadSafeListBase {
public:
    virtual ~ThreadSafeListBase() = default;

    virtual E* insert(E* node) = 0;
    virtual E* remove(E* node) = 0;

    virtual E* insert(const E &value) = 0;
    virtual E* insert(E &&value) = 0;    

    virtual constexpr bool is_blocking() const = 0;
};

#endif
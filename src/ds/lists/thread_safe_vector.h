#ifndef THREAD_SAFE_VECTOR_H
#define THREAD_SAFE_VECTOR_H

#include <vector>
#include <mutex>

// wrapper for vector
template<class E>
class ThreadSafeVector {
public:
    // Default constructor
    ThreadSafeVector() = default;
    
    // Copy constructor
    ThreadSafeVector(const ThreadSafeVector& other) {
        // std::lock_guard<std::mutex> lk(other.lock);
        vec = other.vec;
    }
    
    // Move constructor
    ThreadSafeVector(ThreadSafeVector&& other) noexcept {
        // std::lock_guard<std::mutex> lk(other.lock);
        vec = std::move(other.vec);
    }
    
    // Copy assignment
    ThreadSafeVector& operator=(const ThreadSafeVector& other) {
        if (this != &other) {
            // std::lock(lock, other.lock);
            // std::lock_guard<std::mutex> lk1(lock, std::adopt_lock);
            // std::lock_guard<std::mutex> lk2(other.lock, std::adopt_lock);
            vec = other.vec;
        }
        return *this;
    }
    
    // Move assignment
    ThreadSafeVector& operator=(ThreadSafeVector&& other) noexcept {
        if (this != &other) {
            // std::lock(lock, other.lock);
            // std::lock_guard<std::mutex> lk1(lock, std::adopt_lock);
            // std::lock_guard<std::mutex> lk2(other.lock, std::adopt_lock);
            vec = std::move(other.vec);
        }
        return *this;
    }
    size_t push_back(const E &value) {
        std::lock_guard<std::mutex> lk(lock);
        vec.push_back(value);
        return vec.size();
    }

    // not thread-safe
    bool empty() const {
        return vec.empty();
    }

    // not thread-safe
    void clear() {
        vec.clear();
    }

    // not thread-safe
    const E& operator[](size_t index) const {
        return vec[index];
    }

    // not thread-safe
    E& operator[](size_t index) {
        return vec[index];
    }

    // not thread-safe
    size_t size() const {
        return vec.size();
    }

private:
    std::mutex lock;
    std::vector<E> vec;
};

#endif
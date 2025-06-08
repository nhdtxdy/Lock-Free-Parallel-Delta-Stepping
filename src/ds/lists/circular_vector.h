#ifndef CIRCULAR_VECTOR_H
#define CIRCULAR_VECTOR_H

#include <atomic>
#include <new>
#include <utility>

// Vector that supports concurrent insertion and non-concurrent clear
// Note: use operator new[] and placement new to avoid computation cost when creating std::vector<CircularVector>
// Constructor only called when referred

template<class E>
class CircularVector {
public:
    CircularVector() : data(nullptr), capacity(0) {}
    
    CircularVector(size_t capacity_): capacity(capacity_ + 1) {
        data = static_cast<E*>(operator new[](capacity * sizeof(E)));
    }

    CircularVector(const CircularVector&) = delete;
    CircularVector& operator=(const CircularVector&) = delete;

    CircularVector(CircularVector& other): 
        data(other.data), tail(other.tail.load()), capacity(other.capacity) {
        other.data = nullptr;
        other.tail = 0;
        other.capacity = 0;
    }

    CircularVector(CircularVector&& other) noexcept: 
        data(other.data), tail(other.tail.load()), capacity(other.capacity) {
        other.data = nullptr;
        other.tail = 0;
        other.capacity = 0;
    }

    ~CircularVector() {
        if (data) {
            operator delete[](data);
        }
    }

    CircularVector& operator=(CircularVector& other) {
        if (this != &other) {
            if (data) {
                operator delete[](data);
            }
            
            data = other.data;
            tail.store(other.tail.load());
            capacity = other.capacity;
            
            other.data = nullptr;
            other.tail = 0;
            other.capacity = 0;
        }
        return *this;
    }

    CircularVector& operator=(CircularVector&& other) noexcept {
        if (this != &other) {
            // Clean up current data
            if (data) {
                operator delete[](data);
            }
            
            // Move from other
            data = other.data;
            tail.store(other.tail.load());
            capacity = other.capacity;
            
            other.data = nullptr;
            other.tail = 0;
            other.capacity = 0;
        }
        return *this;
    }

    size_t push(const E &value) {
        size_t current_tail = tail.fetch_add(1);
        new (data + current_tail) E(value);
        return current_tail;
    }

    size_t push(E &&value) {
        size_t current_tail = tail.fetch_add(1);
        new (data + current_tail) E(std::move(value));
        return current_tail;
    }

    void clear() {
        tail = 0;
    }

    const E& operator[](size_t index) const {
        return data[index];        
    }

    E& operator[](size_t index) {
        return data[index];
    }

    bool empty() const {
        return tail == 0;
    }

    size_t size() const {
        return tail;
    }
private:
    E *data;
    std::atomic<size_t> tail{0};
    size_t capacity = 0;
};

#endif
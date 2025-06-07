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
    
    CircularVector(size_t capacity): capacity(capacity) {
        data = static_cast<E*>(operator new[](capacity * sizeof(E)));
    }

    CircularVector(const CircularVector&) = delete;
    CircularVector& operator=(const CircularVector&) = delete;

    CircularVector(CircularVector& other): 
        data(other.data), head(other.head), tail(other.tail.load()), capacity(other.capacity) {
        other.data = nullptr;
        other.head = 0;
        other.tail = 0;
        other.capacity = 0;
    }

    CircularVector(CircularVector&& other) noexcept: 
        data(other.data), head(other.head), tail(other.tail.load()), capacity(other.capacity) {
        other.data = nullptr;
        other.head = 0;
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
            head = other.head;
            tail.store(other.tail.load());
            capacity = other.capacity;
            
            other.data = nullptr;
            other.head = 0;
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
            head = other.head;
            tail.store(other.tail.load());
            capacity = other.capacity;
            
            other.data = nullptr;
            other.head = 0;
            other.tail = 0;
            other.capacity = 0;
        }
        return *this;
    }

    size_t push(const E &value) {
        size_t current_tail = tail.load();
        while (!tail.compare_exchange_weak(current_tail, (current_tail + 1) % capacity));
        size_t relative_idx;
        if (current_tail >= head) {
            relative_idx = current_tail - head;
        }
        else {
            relative_idx = capacity - head + current_tail;
        }
        new (data + current_tail) E(value);
        return relative_idx;
    }

    size_t push(E &&value) {
        size_t current_tail = tail.load();
        while (!tail.compare_exchange_weak(current_tail, (current_tail + 1) % capacity));
        size_t relative_idx;
        if (current_tail >= head) {
            relative_idx = current_tail - head;
        }
        else {
            relative_idx = capacity - head + current_tail;
        }
        new (data + current_tail) E(std::move(value));
        return relative_idx;
    }

    void clear() {
        head = 0;
        tail = 0;
    }

    const E& operator[](size_t index) const {
        size_t real_idx = (head + index) % capacity;
        return data[real_idx];        
    }

    E& operator[](size_t index) {
        size_t real_idx = (head + index) % capacity;
        return data[real_idx];
    }

    bool empty() const {
        return head == tail;
    }

    size_t size() const {
        if (tail >= head) {
            return tail - head;
        }
        else {
            return capacity - head + tail;
        }
    }
private:
    E *data;
    size_t head{0};
    std::atomic<size_t> tail{0};
    size_t capacity = 0;
};

#endif
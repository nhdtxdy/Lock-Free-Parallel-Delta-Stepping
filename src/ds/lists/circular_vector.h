#ifndef CIRCULAR_VECTOR_H
#define CIRCULAR_VECTOR_H

#include <atomic>
#include <vector>

// Vector that supports concurrent insertion and non-concurrent clear

template<class E>
class CircularVector {
public:
    CircularVector() {}
    CircularVector(size_t capacity): data(capacity), capacity(capacity) {}

    CircularVector(const CircularVector& other): 
        data(other.data), head(other.head), tail(other.tail.load()), capacity(other.capacity) {}

    CircularVector(CircularVector&& other) noexcept: 
        data(std::move(other.data)), head(other.head), tail(other.tail.load()), capacity(other.capacity) {}

    CircularVector& operator=(const CircularVector& other) {
        if (this != &other) {
            data = other.data;
            head = other.head;
            tail.store(other.tail.load());
            capacity = other.capacity;
        }
        return *this;
    }

    CircularVector& operator=(CircularVector&& other) noexcept {
        if (this != &other) {
            data = std::move(other.data);
            head = other.head;
            tail.store(other.tail.load());
            capacity = other.capacity;
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
        data[current_tail] = value;
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
        data[current_tail] = std::move(value);
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
    std::vector<E> data;
    size_t head{0};
    std::atomic<size_t> tail{0};
    size_t capacity = 0;
};

#endif
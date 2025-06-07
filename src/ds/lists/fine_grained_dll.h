#ifndef FINE_GRAINED_DLL_H
#define FINE_GRAINED_DLL_H

#include "thread_safe_list_base.h"
#include <mutex>
#include <iostream>
#include <atomic>
#include <cassert>

// Thread safe doubly linked list that supports random insertion/deletion
template<class E>
class FineGrainedDLL : public ThreadSafeListBase<E> {
public:
    struct DLLNode {
        E data;
        DLLNode *next = nullptr;
        DLLNode *prev = nullptr;
        std::mutex lock;
    };

    // Default constructor
    FineGrainedDLL() = default;

    constexpr bool is_lock_free() const {
        return false;
    }

    constexpr bool is_blocking() const {
        return false;
    }

    size_t size() const {
        return _size;
    }

    // Move constructor - required for std::vector operations
    FineGrainedDLL(FineGrainedDLL&& other) noexcept {
        std::lock_guard<std::mutex> lk(other.head_mutex);
        head = other.head;
        other.head = nullptr;
        // head_mutex is default-constructed (new mutex)
    }

    ~FineGrainedDLL() {
        while (head != nullptr) {
            DLLNode *next = head->next;
            delete head;
            head = next;
        }
    }

    DLLNode* insert(const E &value) {
        DLLNode *new_node = new DLLNode;
        new_node->data = value;
        return insert(new_node);
    }

    DLLNode* insert(E &&value) {
        DLLNode *new_node = new DLLNode;
        new_node->data = std::move(value);
        return insert(new_node);
    }

    // insert node before head
    DLLNode* insert(DLLNode *node) {
        if (node == nullptr) {
            return nullptr;
        }
        std::lock_guard<std::mutex> lk(head_mutex);
        if (head != nullptr) {
            std::lock_guard<std::mutex> lk(head->lock);
            node->next = head;
            if (head != nullptr) { // head might have been changed by remove()
                head->prev = node;
            }
        }
        node->prev = nullptr;
        head = node;
        _size.fetch_add(1);
        return node;
    }
    DLLNode* remove(DLLNode *node) {
        if (node == nullptr) {
            return nullptr;
        }

        std::vector<std::unique_lock<std::mutex>> locks;
        locks.reserve(3);
        if (node->prev) {
            locks.emplace_back(node->prev->lock); // equivalent to push_back(std::move(std::lock_guard<std::mutex>))
        }
        locks.emplace_back(node->lock);
        if (node->next) {
            locks.emplace_back(node->next->lock); // equivalent to push_back(std::move(std::lock_guard<std::mutex>))
        }

        if (node->prev != nullptr) {
            node->prev->next = node->next;
        }
        if (node->next != nullptr) {
            node->next->prev = node->prev;
        }
        if (node == head) {
            head = node->next;
        }
        node->prev = nullptr;
        node->next = nullptr;
        _size.fetch_sub(1);
        return node;
    }
    E pop_head() {
        E res = head->data;
        remove(head);
        return res;
    }
    bool empty() const {
        return (head == nullptr);
    }
private:
    DLLNode *head = nullptr;
    std::mutex head_mutex;
    std::atomic<size_t> _size = 0;
};

#endif
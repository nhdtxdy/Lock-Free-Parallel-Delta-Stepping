#ifndef COARSE_GRAINED_DLL_H
#define COARSE_GRAINED_DLL_H

#include "thread_safe_list_base.h"
#include <mutex>

// Thread safe doubly linked list that supports random insertion/deletion
template<class E>
class CoarseGrainedDLL : public ThreadSafeListBase {
public:
    struct DLLNode {
        E data;
        DLLNode *next = nullptr;
        DLLNode *prev = nullptr;
    };

    // not thread-safe
    std::vector<E> list_all_and_clear() {
        std::vector<E> res;
        while (head != nullptr) {
            Node *next = head->next;
            res.emplace_back(std::move(head->data));
            delete head;
            head = next;
        }
        return res; // NRVO -- move semantics applied automatically
    }

    constexpr bool is_lock_free() const override {
        return false;
    }

    constexpr bool is_blocking() const override {
        return false;
    }

    ~CoarseGrainedDLL() {
        while (head != nullptr) {
            DLLNode *next = head->next;
            delete head;
            head = next;
        }
    }
    // insert node before head
    bool insert(DLLNode *node) {
        if (node == nullptr) {
            return false;
        }
        std::lock_guard<std::mutex> lk(global_lock);
        if (head != nullptr) {
            head->prev = node;
        }
        node->next = head;
        node->prev = nullptr;
        head = node;
        return true;
    }
    bool remove(DLLNode *node) {
        if (node == nullptr) {
            return false;
        }
        std::lock_guard<std::mutex> lk(global_lock);
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
        return true;
    }
    bool empty() const {
        return (head == nullptr);
    }
private:
    DLLNode *head = nullptr;
    std::mutex global_lock;
};

#endif
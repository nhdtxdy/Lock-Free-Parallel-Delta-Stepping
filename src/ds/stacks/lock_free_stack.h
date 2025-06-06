#ifndef LOCK_FREE_STACK_H
#define LOCK_FREE_STACK_H

#include "thread_safe_stack_base.h"
#include <atomic>

template<class E>
class LockFreeStack : public ThreadSafeStackBase<E> {
public:
    void push(const E &element) override {
        Node *new_node = new Node;
        new_node->data = element;
        new_node->next = head;
        while (!head.compare_exchange_weak(new_node->next, new_node));
    }
    bool pop(E &res) override {
        Node *old_head = head;
        while (old_head != nullptr && !head.compare_exchange_weak(old_head, old_head->next));
        if (old_head != nullptr) {
            res = old_head->data;
            delete old_head;
            return true;
        }
        return false;
    }
    bool empty() const override {
        return (head == nullptr);
    }
    constexpr bool is_blocking() const override {
        return false;
    }
    constexpr bool is_lock_free() const override {
        return true;
    }
private:
    struct Node {
        E data;
        Node *next = nullptr;
    };

    std::atomic<Node*> head{nullptr};
};

#endif
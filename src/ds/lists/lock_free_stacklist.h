#ifndef LOCK_FREE_STACKLIST_H
#define LOCK_FREE_STACKLIST_H

#include "thread_safe_list_base.h"
#include <stdexcept>

template<class E>
class LockFreeStackList : public ThreadSafeListBase<E> {
public:
    E* insert(const E &element) override {
        Node *new_node = new Node;
        new_node->data = element;
        return insert(new_node);
    }
    E* insert(E &&element) override {
        Node *new_node = new Node;
        new_node->data = std::move(element);
        return insert(new_node);
    }

    E* insert(E* new_node) override {
        new_node->next = head;
        while (!head.compare_exchange_weak(new_node->next, new_node));
        _size.fetch_add(1);
        return new_node;
    }

    E* remove(E* node) override {
        throw std::logic_error("LockFreeStackList does not support arbitrary node removal.");
        return nullptr;
    }

    bool pop(E &res) {
        Node *old_head = head;
        while (old_head != nullptr && !head.compare_exchange_weak(old_head, old_head->next));
        if (old_head != nullptr) {
            res = old_head->data;
            delete old_head;
            _size.fetch_sub(1);
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

    size_t size() const {
        return _size;
    }

private:
    struct Node {
        E data;
        Node *next = nullptr;
    };

    std::atomic<Node*> head{nullptr};
    std::atomic<size_t> _size{0};
};

#endif
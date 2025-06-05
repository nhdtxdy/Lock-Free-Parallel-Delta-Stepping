#ifndef MIN_LOCK_QUEUE_H
#define MIN_LOCK_QUEUE_H

#ifndef LOCK_FREE_QUEUE_H
#define LOCK_FREE_QUEUE

#include "thread_safe_queue_base.h"
#include <mutex>

// TO STUDY: USE SHARED_PTR INSTEAD (HOW MUCH SLOWER WOULD IT BE?)
template<class E>
class MinLockQueue : public ThreadSafeQueueBase {
public:
    MinLockQueue();
    ~MinLockQueue();

    // Move semantics
    MinLockQueue(MinLockQueue &&other) noexcept;
    MinLockQueue& operator=(MinLockQueue &&other) noexcept;

    void push(const E &value) override;
    bool pop(E &res) override;

    constexpr bool is_blocking() const override {
        return true;
    }

private:
    struct Node {
        E data;
        std::atomic<Node*> next = nullptr;
    };
    
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    std::condition_variable not_empty;
    std::mutex lock;
};

template<class E>
MinLockQueue<E>::MinLockQueue() {
    Node *tmp = new Node;
    head = tmp;
    tail = tmp;
}

template<class E>
MinLockQueue<E>::~MinLockQueue() {
    while (Node *old_head = head) {
        head = old_head->next;
        delete old_head;
    }
}

template<class E>
void MinLockQueue<E>::push(const E &value) {
    Node *node = new Node;
    node->data = value;
    Node *old_tail;

    // Michael-Scott's (1996)
    while (true) {
        old_tail = tail;
        Node *tail_next = old_tail->next;
        if (old_tail == tail) {
            if (tail_next == nullptr) {
                if (old_tail->next.compare_exchange_weak(tail_next, node)) {
                    break;
                }
            }
            else {
                tail.compare_exchange_weak(old_tail, tail_next);
            }
        }
    }
    // while (!tail.compare_exchange_weak(old_tail, node));
    // tail.compare_exchange_strong(old_tail, node);
    tail.compare_exchange_weak(old_tail, node);
    
    {
        std::lock_guard<std::mutex> lk(lock);
        not_empty.notify_one();
    }

    // would this code work? could be a more succinct alternative to Michael-Scott's algorithm
    // while (true) {
    //     old_tail = tail;
    //     if (old_tail->next.compare_exchange_weak(nullptr, new_node)) {
    //         if (tail.compare_exchange_strong(old_tail, new_node)) {
    //             return;
    //         }
    //     }
    // }
}

template<class E>
bool MinLockQueue<E>::pop(E &res) {
    Node *old_head, *old_tail, *next;
    while (true) {
        old_head = head;
        old_tail = tail;
        next = old_head->next;
        if (old_head == head) {
            if (old_head == old_tail) {
                if (next == nullptr) { // queue is empty
                    std::unique_lock<std::mutex> lk(lock);
                    not_empty.wait(lk); // TO STUDY: add predicate?
                    continue;
                }
                tail.compare_exchange_weak(old_tail, next);
            }
            else {
                if (head.compare_exchange_weak(old_head, next)) {
                    res = std::move(head->next->data);
                    delete old_head;
                    return true;
                }
            }
        }
    }
}

template<class E>
MinLockQueue<E>::MinLockQueue(MinLockQueue&& other) noexcept {
    head = other.head;
    tail = other.tail;
    other.head = nullptr;
    other.tail = nullptr;
    other.exponential_backoff_retries = 0;
}

template<class E>
MinLockQueue<E>& MinLockQueue<E>::operator=(MinLockQueue&& other) noexcept {
    if (this != &other) {
        // Clean up current queue nodes
        Node *node = head;
        while (node) {
            Node* next = node->next;
            delete node;
            node = next;
        }

        head = other.head;
        tail = other.tail;
        other.head = nullptr;
        other.tail = nullptr;
        other.exponential_backoff_retries = 0;
    }
    return *this;
}

#endif

#endif
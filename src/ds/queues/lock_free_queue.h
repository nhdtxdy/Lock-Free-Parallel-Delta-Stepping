#ifndef LOCK_FREE_QUEUE_H
#define LOCK_FREE_QUEUE

#include "thread_safe_queue_base.h"

// TO STUDY: USE SHARED_PTR INSTEAD (HOW MUCH SLOWER WOULD IT BE?)
template<class E>
class LockFreeQueue : public ThreadSafeQueueBase {
public:
    LockFreeQueue(int backoff_retries);
    ~LockFreeQueue();

    // Move semantics
    LockFreeQueue(LockFreeQueue &&other) noexcept;
    LockFreeQueue& operator=(LockFreeQueue &&other) noexcept;

    void push(const E &value) override;
    bool pop(E &res) override;
    constexpr bool is_blocking() const override {
        return false;
    }
    constexpr bool is_lock_free() const override {
        return true;
    }

    bool empty() const override {
        return (head->next == nullptr);
    }

private:
    struct Node {
        E data;
        std::atomic<Node*> next = nullptr;
    };
    
    int exponential_backoff_retries = 0; // 0 means backoff is disabled
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
};

template<class E>
LockFreeQueue<E>::LockFreeQueue(int backoff_retries = -1): exponential_backoff_retries(backoff_retries) {
    Node *tmp = new Node;
    head = tmp;
    tail = tmp;
}

template<class E>
LockFreeQueue<E>::~LockFreeQueue() {
    while (Node *old_head = head) {
        head = old_head->next;
        delete old_head;
    }
}

template<class E>
void LockFreeQueue<E>::push(const E &value) {
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

    // QUESTION: would this code work?
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
bool LockFreeQueue<E>::pop(E &res) {
    // Michael-Scott's (1996)
    Node *old_head, *old_tail, *next;
    int retries = 1;
    while (true) {
        old_head = head;
        old_tail = tail;
        next = old_head->next;
        if (old_head == head) {
            if (old_head == old_tail) {
                if (next == nullptr) { // queue is empty
                    if (retries < exponential_backoff_retries) {
                        for (int i = 0; i < retries; ++i) {
                            std::this_thread::yield();
                        }
                        retries <<= 1;
                        continue;
                    }
                    return false;
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
LockFreeQueue<E>::LockFreeQueue(LockFreeQueue&& other) noexcept {
    head = other.head;
    tail = other.tail;
    exponential_backoff_retries = other.exponential_backoff_retries;
    other.head = nullptr;
    other.tail = nullptr;
    other.exponential_backoff_retries = 0;
}

template<class E>
LockFreeQueue<E>& LockFreeQueue<E>::operator=(LockFreeQueue&& other) noexcept {
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
        exponential_backoff_retries = other.exponential_backoff_retries;
        other.head = nullptr;
        other.tail = nullptr;
        other.exponential_backoff_retries = 0;
    }
    return *this;
}

#endif
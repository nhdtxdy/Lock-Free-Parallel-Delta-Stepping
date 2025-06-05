#ifndef HEAD_TAIL_LOCK_QUEUE_NONBLOCKING_H
#define HEAD_TAIL_LOCK_QUEUE_NONBLOCKING_H

#include "thread_safe_queue_base.h"
#include <mutex>

template<class E>
class HeadTailLockQueueNonBlocking : public ThreadSafeQueueBase {
public:
    HeadTailLockQueueNonBlocking();
    ~HeadTailLockQueueNonBlocking();

    // move semantics
    HeadTailLockQueueNonBlocking(HeadTailLockQueueNonBlocking &&other) noexcept;
    HeadTailLockQueueNonBlocking& operator=(HeadTailLockQueueNonBlocking &&other) noexcept;

    void push(const E &element) override;
    bool pop(E &res) override;

    constexpr bool is_blocking() const override {
        return false;
    }

    constexpr bool is_lock_free() const override {
        return false;
    }

    bool empty() const {
        return (head->next == nullptr);
    }

private:
    Node *head;
    Node *tail;
    
    std::mutex head_lock;
    std::mutex tail_lock;

protected:
    struct Node {
        E data;
        Node *next = nullptr;
    };
};

template<class E>
HeadTailLockQueueNonBlocking<E>::HeadTailLockQueueNonBlocking() {
    Node *node = new Node;
    head = node;
    tail = node;
}

template<class E>
HeadTailLockQueueNonBlocking<E>::~HeadTailLockQueueNonBlocking() {
    while (Node *old_head = head) {
        head = old_head->next;
        delete old_head;
    }
}

template<class E>
void HeadTailLockQueueNonBlocking<E>::push(const E &element) {
    Node *node = new Node;
    node->data = element;
    {
        std::lock_guard<std::mutex> lk(tail_lock);
        tail->next = node;
        tail = node;
    }
}

template<class E>
bool HeadTailLockQueueNonBlocking<E>::pop(E &res) {
    std::unique_lock<std::mutex> lk(head_lock);
    Node *node = head;
    Node *new_head = node->next;
    if (new_head == nullptr) {
        return false;
    }
    res = std::move(new_head->data);
    head = new_head;
    lk.unlock();
    delete node;
    return true;
}

template<class E>
HeadTailLockQueueNonBlocking<E>::HeadTailLockQueueNonBlocking(HeadTailLockQueueNonBlocking &&other) noexcept {
    std::scoped_lock(head_lock, tail_lock, other.head_lock, other.tail_lock);
    head = other.head;
    tail = other.tail;
    other.head = nullptr;
    other.tail = nullptr;
}

template<class E>
HeadTailLockQueueNonBlocking<E>& HeadTailLockQueueNonBlocking<E>::operator=(HeadTailLockQueueNonBlocking &&other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(head_lock, tail_lock, other.head_lock, other.tail_lock);
        
        while (Node *old_head = head) {
            head = old_head->next;
            delete old_head;
        }
        
        head = other.head;
        tail = other.tail;
        other.head = nullptr;
        other.tail = nullptr;
    }
    return *this;
}


#endif
#ifndef HEAD_TAIL_LOCK_QUEUE_BLOCKING_H
#define HEAD_TAIL_LOCK_QUEUE_BLOCKING_H

#include "head_tail_lock_queue_nonblocking.h"

template<class E>
class HeadTailLockQueueBlocking : public HeadTailLockQueueNonBlocking<E> {
public:
    using Node = typename HeadTailLockQueueNonBlocking<E>::Node;
    using HeadTailLockQueueNonBlocking<E>::head_lock;
    using HeadTailLockQueueNonBlocking<E>::tail_lock;
    using HeadTailLockQueueNonBlocking<E>::head;
    using HeadTailLockQueueNonBlocking<E>::tail;
    using HeadTailLockQueueNonBlocking<E>::HeadTailLockQueueNonBlocking;
    void push(const E &element) override;
    bool pop(E &res) override;
    static constexpr bool is_blocking() {
        return true;
    }
private:
    std::condition_variable not_empty;
};

template<class E>
void HeadTailLockQueueBlocking<E>::push(const E &element) {
    Node *node = new Node;
    node->data = element;
    {
        std::lock_guard<std::mutex> lk(tail_lock);
        tail->next = node;
        tail = node;
        not_empty.notify_one();
    }
}

template<class E>
bool HeadTailLockQueueBlocking<E>::pop(E &res) {
    std::unique_lock<std::mutex> lk(head_lock);
    while (head->next == nullptr) {
        not_empty.wait(lk);
    }
    Node *node = head;
    Node *new_head = node->next;
    res = std::move(new_head->data);
    head = new_head;
    lk.unlock();
    delete node;
    return true;
}

#endif
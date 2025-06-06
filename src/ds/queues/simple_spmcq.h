#ifndef SIMPLE_SPMCQ_H
#define SIMPLE_SPMCQ_H

#include "thread_safe_queue_base.h"

// TO STUDY: USE SHARED_PTR INSTEAD (HOW MUCH SLOWER WOULD IT BE?)
template<class E>
class SimpleSPMCQueue : public ThreadSafeQueueBase<E> {
public:
    SimpleSPMCQueue() {
        Node *tmp = new Node;
        head.store(tmp);
        tail.store(tmp);
    }
    ~SimpleSPMCQueue() {
        E tmp;
        while (pop(tmp));
        delete head.load();
    }

    // NOT THREAD-SAFE    
    void push(const E &value) override {
        Node *node = new Node;
        node->data = value;

        Node *old_tail = tail.load();
        old_tail->next.store(node);
        tail.store(node);
    }

    void push(E &&value) {
        Node *node = new Node;
        node->data = std::move(value);

        Node *old_tail = tail.load();
        old_tail->next.store(node);
        tail.store(node);
    }

    bool pop(E &res) override {
        Node *head_next;
        Node *old_head;
        while (true) {
            old_head = head.load();
            head_next = old_head->next.load();

            if (head_next == nullptr) {
                return false;
            }

            if (head.compare_exchange_weak(old_head, head_next)) {
                res = head_next->data;
                // delete old_head;
                return true;
            }
        }
    }

    static constexpr bool is_blocking() {
        return false;
    }

    bool empty() const override {
        return (head.load()->next.load() == nullptr);
    }

private:
    struct Node {
        E data;
        std::atomic<Node*> next{nullptr};
        // Node(const E& value) : data(value), next(nullptr) {}
        // Node(): next(nullptr) {}
    };
    
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
};


#endif
#ifndef TWO_STACKS_QUEUE_BLOCKING_H
#define TWO_STACKS_QUEUE_BLOCKING_H

#include "two_stacks_queue.h"

template<class E>
class TwoStacksQueueBlocking : public TwoStacksQueue {
public:
    using TwoStacksQueue::TwoStacksQueue;
    void push(const E &element) {
        s1.push(element);
        is_transferring.notify_one();
    }
    bool pop(E &res) {
        while (true) {
            if (s2.pop(res)) {
                return true;
            }

            bool expected = false;
            if (is_transferring.compare_exchange_strong(expected, true)) {
                E tmp;
                while (s1.pop(tmp)) {
                    s2.push(tmp);
                }
                is_transferring = false;
                is_transferring.notify_one();

                if (s2.pop(res)) {
                    return true;
                }
            }
            if (s1.empty() && s2.empty()) {
                is_transferring.wait(true);
            }
        }
        return false;
    }
    constexpr bool is_lock_free() const override {
        return true;
    }
};

#endif
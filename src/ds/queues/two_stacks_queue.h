#ifndef TWO_STACKS_QUEUE_H
#define TWO_STACKS_QUEUE_H

#include "stacks/lock_free_stack.h"
#include "thread_safe_queue_base.h"

// non-blocking

template<class E>
class TwoStacksQueue : public ThreadSafeStackBase<E> {
public:
    void push(const E &element) {
        s1.push(element);
    }
    void push(E &&element) {
        s1.push(std::move(element));
    }
    bool pop(E &res) {
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
        }
        is_transferring.wait(true);
        return s2.pop(res);
    }
    bool empty() const {
        return s2.empty() && s1.empty();
    }
    static constexpr bool is_blocking() {
        return false;
    }
private:
    LockFreeStack<E> s1, s2;
    std::atomic<bool> is_transferring;
};

#endif
// Copyright 2010-2015 RethinkDB, all rights reserved.
#ifndef CONCURRENCY_EXPONENTIAL_BACKOFF_HPP_
#define CONCURRENCY_EXPONENTIAL_BACKOFF_HPP_

#include "arch/timing.hpp"

#include "time.hpp"

class exponential_backoff_t {
public:
    exponential_backoff_t(
            chrono::milliseconds _mnbm, chrono::milliseconds _mxbm, double _ff = 1.5, double _sf = 0.0) :
        min_backoff_ms(_mnbm), max_backoff_ms(_mxbm), fail_factor(_ff),
        success_factor(_sf), backoff_ms(0)
        { }
    void failure(signal_t *interruptor) {
        if (backoff_ms == chrono::milliseconds::zero()) {
            coro_t::yield();
            backoff_ms = min_backoff_ms;
        } else {
            nap(backoff_ms, interruptor);
            guarantee((backoff_ms * fail_factor) > backoff_ms,
                "rounding screwed it up");
            backoff_ms *= fail_factor;
            if (backoff_ms > max_backoff_ms) {
                backoff_ms = max_backoff_ms;
            }
        }
    }
    void success() {
        guarantee((backoff_ms * success_factor) < backoff_ms
            || backoff_ms == chrono::milliseconds::zero(), "rounding screwed it up");
        backoff_ms *= success_factor;
        if (backoff_ms < min_backoff_ms) {
            backoff_ms = chrono::milliseconds::zero();
        }
    }
private:
    const chrono::milliseconds min_backoff_ms, max_backoff_ms;
    const double fail_factor, success_factor;
    chrono::milliseconds backoff_ms;
};

#endif /* CONCURRENCY_EXPONENTIAL_BACKOFF_HPP_ */


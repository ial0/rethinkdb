// Copyright 2010-2014 RethinkDB, all rights reserved.
#include "arch/timing.hpp"

#include <algorithm>
#include <functional>

#include "arch/arch.hpp"
#include "arch/runtime/coroutines.hpp"
#include "concurrency/wait_any.hpp"

#include "time.hpp"

// nap()

void nap(milli_t ms) THROWS_NOTHING {
    if (ms > milli_t::zero()) {
        signal_timer_t timer;
        timer.start(ms);
        timer.wait_lazily_ordered();
    }
}

void nap(milli_t ms, const signal_t *interruptor) THROWS_ONLY(interrupted_exc_t) {
    signal_timer_t timer(ms);
    wait_interruptible(&timer, interruptor);
}

// signal_timer_t

signal_timer_t::signal_timer_t() : timer(nullptr) { }
signal_timer_t::signal_timer_t(milli_t ms) : timer(nullptr) {
    start(ms);
}

signal_timer_t::~signal_timer_t() {
    if (timer != nullptr) {
        cancel_timer(timer);
    }
}

void signal_timer_t::start(milli_t ms) {
    guarantee(timer == nullptr);
    guarantee(!is_pulsed());
    if (ms == milli_t::zero()) {
        pulse();
    } else {
        guarantee(ms > milli_t::zero());
        timer = fire_timer_once(ms, this);
    }
}

bool signal_timer_t::cancel() {
    if (timer != nullptr) {
        cancel_timer(timer);
        timer = nullptr;
        return true;
    }
    return false;
}

bool signal_timer_t::is_running() const {
    return is_pulsed() || timer != nullptr;
}

void signal_timer_t::on_timer(monotonic_t) {
    timer = nullptr;
    pulse();
}

// repeating_timer_t

repeating_timer_t::repeating_timer_t(
        milli_t interval, const std::function<void()> &_ringee) :
    interval(interval),
    last_time(clock_monotonic()),
    expected_next(last_time + interval),
    ringee(_ringee) {
    rassert(interval > milli_t::zero());
    timer = add_timer(interval, this);
}

repeating_timer_t::repeating_timer_t(
        milli_t interval, repeating_timer_callback_t *_cb) :
    interval(interval),
    last_time(clock_monotonic()),
    expected_next(last_time + interval),
    ringee([_cb]() { _cb->on_ring(); }) {
    rassert(interval > milli_t::zero());
    timer = add_timer(interval, this);
}

repeating_timer_t::~repeating_timer_t() {
    cancel_timer(timer);
}

void repeating_timer_t::change_interval(milli_t interval_ms) {
    if (interval_ms == interval) {
        return;
    }

    interval = interval_ms;
    cancel_timer(timer);
    expected_next = std::min(last_time + interval_ms, expected_next);
    timer = add_timer2(expected_next, interval_ms, this);
}

void repeating_timer_t::clamp_next_ring(milli_t delay) {
    auto t = last_time + delay;
    if (t < expected_next) {
        cancel_timer(timer);
        expected_next = t;
        timer = add_timer2(expected_next, interval, this);
    }
}

void call_ringer(std::function<void()> ringee) {
    // It would be very easy for a user of repeating_timer_t to have
    // object lifetime errors, if their ring function blocks.  So we
    // have this assertion.
    ASSERT_FINITE_CORO_WAITING;
    ringee();
}

void repeating_timer_t::on_timer(monotonic_t time) {
    // Spawn _now_, otherwise the repeating_timer_t lifetime might end
    // before ring gets used.
    last_time = time;
    expected_next = last_time + interval;
    coro_t::spawn_now_dangerously(std::bind(call_ringer, ringee));
}

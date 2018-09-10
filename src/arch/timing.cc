// Copyright 2010-2014 RethinkDB, all rights reserved.
#include "arch/timing.hpp"

#include <algorithm>
#include <functional>

#include "arch/arch.hpp"
#include "arch/runtime/coroutines.hpp"
#include "concurrency/wait_any.hpp"

#include "time.hpp"

// nap()

void nap(chrono::milliseconds ms) THROWS_NOTHING {
    if (ms > chrono::milliseconds::zero()) {
        signal_timer_t timer;
        timer.start(ms);
        timer.wait_lazily_ordered();
    }
}

void nap(chrono::milliseconds ms, const signal_t *interruptor) THROWS_ONLY(interrupted_exc_t) {
    signal_timer_t timer(ms);
    wait_interruptible(&timer, interruptor);
}

// signal_timer_t

signal_timer_t::signal_timer_t() : timer(nullptr) { }
signal_timer_t::signal_timer_t(chrono::milliseconds ms) : timer(nullptr) {
    start(ms);
}

signal_timer_t::~signal_timer_t() {
    if (timer != nullptr) {
        cancel_timer(timer);
    }
}

void signal_timer_t::start(chrono::milliseconds ms) {
    guarantee(timer == nullptr);
    guarantee(!is_pulsed());
    if (ms == chrono::milliseconds::zero()) {
        pulse();
    } else {
        guarantee(ms > chrono::milliseconds::zero());
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
        chrono::milliseconds interval, const std::function<void()> &_ringee) :
    interval(interval),
    last_time(clock_monotonic()),
    expected_next(last_time + interval),
    ringee(_ringee) {
    rassert(interval > chrono::milliseconds::zero());
    timer = add_timer(interval, this);
}

repeating_timer_t::repeating_timer_t(
        chrono::milliseconds interval, repeating_timer_callback_t *_cb) :
    interval(interval),
    last_time(clock_monotonic()),
    expected_next(last_time + interval),
    ringee([_cb]() { _cb->on_ring(); }) {
    rassert(interval > chrono::milliseconds::zero());
    timer = add_timer(interval, this);
}

repeating_timer_t::~repeating_timer_t() {
    cancel_timer(timer);
}

void repeating_timer_t::change_interval(chrono::milliseconds interval_ms) {
    if (interval_ms == interval) {
        return;
    }

    interval = interval_ms;
    cancel_timer(timer);
    expected_next = std::min(last_time + interval_ms, expected_next);
    timer = add_timer2(expected_next, interval_ms, this);
}

void repeating_timer_t::clamp_next_ring(chrono::milliseconds delay) {
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

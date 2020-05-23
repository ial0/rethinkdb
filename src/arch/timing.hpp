// Copyright 2010-2014 RethinkDB, all rights reserved.
#ifndef ARCH_TIMING_HPP_
#define ARCH_TIMING_HPP_

#include <functional>

#include "arch/timer.hpp"
#include "concurrency/interruptor.hpp"
#include "concurrency/signal.hpp"

/* Coroutine function that delays for some number of milliseconds. */

void nap(chrono::milliseconds ms) THROWS_NOTHING;

/* This variant takes an interruptor, and throws `interrupted_exc_t` if the
interruptor is pulsed before the timeout is up */

void nap(chrono::milliseconds ms, const signal_t *interruptor) THROWS_ONLY(interrupted_exc_t);

class timer_token_t;

/* Construct a `signal_timer_t` to start a one-shot timer. When the timer "rings",
the signal will be pulsed. It is safe to destroy the `signal_timer_t` before the
timer "rings". */

class signal_timer_t : public signal_t, private timer_callback_t {
public:
    signal_timer_t();
    explicit signal_timer_t(chrono::milliseconds ms); // Calls `start` for you.
    ~signal_timer_t();

    // Starts the timer, cannot be called if the timer is already running
    void start(chrono::milliseconds ms);

    // Stops the timer from running
    // Returns true if the timer was canceled, false if there was no timer to cancel
    bool cancel();

    // Returns true if the timer is running or has been pulsed
    bool is_running() const;

private:
    void on_timer(monotonic_t time) override;
    timer_token_t *timer;
};

/* Construct a repeating_timer_t to start a repeating timer. It will call its function
when the timer "rings". */

/* `repeating_timer_callback_t` is deprecated; prefer passing a function to
`repeating_timer_t`. */
class repeating_timer_callback_t {
public:
    virtual void on_ring() = 0;
protected:
    virtual ~repeating_timer_callback_t() { }
};

class repeating_timer_t : private timer_callback_t {
public:
    repeating_timer_t(chrono::milliseconds interval_ms, const std::function<void()> &ringee);
    repeating_timer_t(chrono::milliseconds interval_ms, repeating_timer_callback_t *ringee);
    ~repeating_timer_t();

    // Increases or decreases the interval.  The next ring of the timer will always be
    // based on the minimum value of the timing interval held before that ring.
    void change_interval(chrono::milliseconds interval_ms);

    // Makes next ring happen no later than delay_ms milliseconds from now (or slightly
    // later).
    void clamp_next_ring(chrono::milliseconds delay_ms);

    chrono::milliseconds interval_ms() const { return interval; }

private:
    void on_timer(monotonic_t ticks) override;
    chrono::milliseconds interval;  // (milliseconds)
    monotonic_t last_time;
    monotonic_t expected_next;
    timer_token_t *timer;
    std::function<void()> ringee;

    DISABLE_COPYING(repeating_timer_t);
};

#endif /* ARCH_TIMING_HPP_ */

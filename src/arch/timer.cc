// Copyright 2010-2014 RethinkDB, all rights reserved.
#include "arch/timer.hpp"

#include <algorithm>

#include <memory>

#include "arch/runtime/thread_pool.hpp"
#include "time.hpp"
#include "utils.hpp"

class timer_token_t : public intrusive_priority_queue_node_t<timer_token_t> {
    friend class timer_handler_t;

private:
    timer_token_t() : interval(nano_t{-1}), next_time(nano_t{-1}), callback(nullptr) { }

    friend bool left_is_higher_priority(const timer_token_t *left, const timer_token_t *right);

    // The time between rings, if a repeating timer, otherwise zero.
    ticks_t interval;

    // The time of the next 'ring'.
    monotonic_t next_time;

    // The callback we call upon each 'ring'.
    timer_callback_t *callback;

    DISABLE_COPYING(timer_token_t);
};

bool left_is_higher_priority(const timer_token_t *left, const timer_token_t *right) {
    return left->next_time < right->next_time;
}

timer_handler_t::timer_handler_t(linux_event_queue_t *queue)
    : timer_provider(queue),
      expected_oneshot_time(nano_t::zero()) {
    // Right now, we have no tokens.  So we don't ask the timer provider to do anything for us.
}

timer_handler_t::~timer_handler_t() {
    guarantee(token_queue.empty());
}

void timer_handler_t::on_oneshot() {
    // If the timer_provider tends to return its callback a touch early, we don't want to make a
    // bunch of calls to it, returning a tad early over and over again, leading up to a ticks
    // threshold.  So we bump the real time up to the threshold when processing the priority queue.
    auto realtime = clock_monotonic();
    auto time = std::max(realtime, expected_oneshot_time);

    while (!token_queue.empty() && token_queue.peek()->next_time <= time) {
        std::unique_ptr<timer_token_t> token{token_queue.pop()};

        // Put the repeating timer back on the queue before the callback can be called (so that it
        // may be canceled).
        if (token->interval != nano_t::zero()) {
            token->next_time = realtime + token->interval;
            token_queue.push(token.get());
        }

        token->callback->on_timer(realtime);

        // Delete nonrepeating timer tokens.
        if (token->interval != nano_t::zero()) {
            token.release();
        }
    }

    // We've processed young tokens.  Now schedule a new one-shot (if necessary).
    if (!token_queue.empty()) {
        timer_provider.schedule_oneshot(token_queue.peek()->next_time, this);
    }
}

timer_token_t *timer_handler_t::add_timer_internal(
        const monotonic_t next_time, const milli_t interval,
        timer_callback_t *callback) {
	rassert(next_time >= monotonic_t{});
    rassert(interval >= milli_t::zero());

    std::unique_ptr<timer_token_t> token{new timer_token_t{}};
    token->interval = interval;
    token->next_time = next_time;
    token->callback = callback;

    const timer_token_t *top_entry = token_queue.peek();
    token_queue.push(token.get());

    if (top_entry == nullptr || next_time < top_entry->next_time) {
        timer_provider.schedule_oneshot(next_time, this);
    }

    return token.release();
}

void timer_handler_t::cancel_timer(timer_token_t *token) {
    token_queue.remove(token);
    delete token;

    if (token_queue.empty()) {
        timer_provider.unschedule_oneshot();
    }
}



timer_token_t *add_timer2(monotonic_t next_time, milli_t interval,
                          timer_callback_t *callback) {
    rassert(interval > milli_t::zero());
    return linux_thread_pool_t::get_thread()->timer_handler.add_timer_internal(
        next_time, interval, callback);
}

timer_token_t *add_timer(milli_t ms, timer_callback_t *callback) {
    rassert(ms > milli_t::zero());
    auto next_time = clock_monotonic() + ms;
    return linux_thread_pool_t::get_thread()->timer_handler.add_timer_internal(
       next_time, ms, callback);
}

timer_token_t *fire_timer_once(milli_t ms, timer_callback_t *callback) {
    auto next_time = clock_monotonic() + ms;
    return linux_thread_pool_t::get_thread()->timer_handler.add_timer_internal(
        next_time, milli_t::zero(), callback);
}

void cancel_timer(timer_token_t *timer) {
    linux_thread_pool_t::get_thread()->timer_handler.cancel_timer(timer);
}

// Copyright 2010-2014 RethinkDB, all rights reserved.
#include <algorithm>

#include "arch/timing.hpp"
#include "concurrency/pmap.hpp"
#include "unittest/gtest.hpp"
#include "unittest/unittest_utils.hpp"
#include "utils.hpp"

namespace unittest {

const int waits = 10;
const int simultaneous = 2;
const int repeat = 10;

const int wait_array[simultaneous][waits] =
    { { 1, 1, 2, 3, 5, 13, 20, 30, 40, 8 },
      { 5, 3, 2, 40, 30, 20, 8, 13, 1, 1 } };

#ifdef _WIN32
// Assuming a 15ms sleep resolution
const milli_t max_error_ms{16};
const int max_average_error_ms = 11;
#else
const milli_t max_error_ms{5};
const int  max_average_error_ms = 2;
#endif

auto nsabs(ticks_t t)
{
	return t > ticks_t::zero() ? t : -t;
}

void walk_wait_times(int i, uint64_t *mse) {
    uint64_t se = 0;
    for (int j = 0; j < waits; ++j) {
        auto expected_ms = milli_t{wait_array[i][j]};
        ticks_t t1 = get_ticks();
        nap(expected_ms);
        ticks_t t2 = get_ticks();
        auto actual_ns = t2 - t1;
        auto error_ns = actual_ns - expected_ms;

        EXPECT_LT(nsabs(error_ns), max_error_ms)
            << "failed to nap for " << expected_ms.count() << "ms";

        se += error_ns.count() * error_ns.count();
    }
    *mse += se / waits;
}

TPTEST(TimerTest, TestApproximateWaitTimes) {
    uint64_t mse_each[simultaneous] = {0};
    for (int i = 0; i < repeat; i++){
        pmap(simultaneous, [&](int j){ walk_wait_times(j, &mse_each[j]); });
    }
    int64_t mse = 0;
    for (int i = 0; i < simultaneous; i++) {
        mse += mse_each[i] / repeat;
    }
    mse /= simultaneous;
    EXPECT_LT(sqrt(mse) / MILLION, max_average_error_ms)
        << "Average timer error too high";
}

TPTEST(TimerTest, TestRepeatingTimer) {
    auto first_ticks = get_ticks();
    int count = 0;
    repeating_timer_t timer(milli_t{30}, [&]() {
        ++count;
        auto ticks = get_ticks();
        auto diff = ticks - first_ticks;
        EXPECT_LT(nsabs(diff - milli_t{30} * count), max_error_ms);
    });
    nap(milli_t{100});
}

TPTEST(TimerTest, TestChangeInterval) {
    ticks_t first_ticks = get_ticks();
    int count = 0;
    const int64_t expected[] = { 5, 10, 20, 40, 65 };
    const int64_t naps[] = {0,  0,  0,  25, 0};
    const int64_t ms[] = { 10, 20, 30, 10, 50};
    scoped_ptr_t<repeating_timer_t> timer;
    timer = make_scoped<repeating_timer_t>(milli_t{10}, [&]() {
        coro_t::spawn_now_dangerously([&]() {
            ASSERT_LT(count, 5);
            ticks_t ticks = get_ticks();
            auto diff = ticks - first_ticks;
            EXPECT_LT(nsabs(diff - milli_t{expected[count]}),
                      max_error_ms);
            nap(milli_t{naps[count]});
            timer->change_interval(milli_t{ms[count]});
            ++count;
        });
    });
    timer->change_interval(milli_t{5});
    nap(milli_t{70});
}


}  // namespace unittest

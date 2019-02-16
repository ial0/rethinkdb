#ifndef TIME_HPP_
#define TIME_HPP_

#include <stdint.h>
#include <time.h>

#include <chrono>

#include "version.hpp"
#include "containers/archive/archive.hpp"

using monotonic_t = std::chrono::steady_clock::time_point;
using realtime_t = std::chrono::system_clock::time_point;

struct timespec_t {
    realtime_t _time;
    std::chrono::nanoseconds _remaining;

    timespec_t() : _time{realtime_t::min()} {}

    timespec_t(realtime_t t) : _time(t) {}

    realtime_t time() const {
        return _time;
    }
    
    std::chrono::nanoseconds nanoseconds() const {
        return  _time.time_since_epoch()  - 
            std::chrono::duration_cast<std::chrono::seconds>(_time.time_since_epoch());
    }

    friend bool operator==(timespec_t const &a, timespec_t const &b) {
        return a._time == b._time;
    }

    friend bool operator<(timespec_t const &a, timespec_t const &b) {
        return a._time < b._time;
    }

    friend bool operator<=(timespec_t const &a, timespec_t const &b) {
        return a._time <= b._time;
    }

    friend bool operator>(timespec_t const &a, timespec_t const &b) {
        return a._time > b._time;
    }
};

struct uptime_t {
    std::chrono::nanoseconds uptime;

    uptime_t() : uptime{std::chrono::nanoseconds::min()} {}

    uptime_t(std::chrono::nanoseconds u) : uptime(u) {}

    std::chrono::nanoseconds time() const {
        return uptime;
    }

    friend bool operator==(uptime_t const &a, uptime_t const &b) {
        return a.uptime == b.uptime;
    }

    friend bool operator<(uptime_t const &a, uptime_t const &b) {
        return a.uptime < b.uptime;
    }

    friend bool operator>(uptime_t const &a, uptime_t const &b) {
        return a.uptime > b.uptime;
    }
};

inline monotonic_t clock_monotonic()
{
    return std::chrono::steady_clock::now();
}

inline realtime_t clock_realtime()
{
    return std::chrono::system_clock::now();
}

template <cluster_version_t W>
void serialize(write_message_t *wm, const monotonic_t &s) {
    serialize<W>(wm, std::chrono::nanoseconds{s.time_since_epoch()});
}

template <cluster_version_t W>
MUST_USE archive_result_t deserialize(read_stream_t *s, monotonic_t *p) {
    std::chrono::nanoseconds d;
    archive_result_t res = deserialize<W>(s, &d);
    if (bad(res)) { return res; }
    *p = monotonic_t{std::chrono::nanoseconds{d}};
    return res;
}

using kiloticks_t = std::chrono::duration<int64_t, std::micro>;
using ticks_t = std::chrono::duration<int64_t, std::nano>;

namespace chrono {
	using namespace std::chrono;

	using hours  = std::chrono::duration<int64_t, std::ratio<3600>>;
	using days = std::chrono::duration<int64_t, std::ratio<86400>>;

    template <class To, class Rep, class Period>
    constexpr To ceil(const std::chrono::duration<Rep, Period>& d)
    {
        return std::chrono::duration_cast<To>(d) < d ?
            std::chrono::duration_cast<To>(d) + To{1} : std::chrono::duration_cast<To>(d);
    }
}

using datum_seconds_t = std::chrono::duration<double>;
using datum_milli_t = std::chrono::duration<double, std::milli>;
using datum_micro_t = std::chrono::duration<double, std::micro>;

template <cluster_version_t W, typename R>
void serialize(write_message_t *wm, const std::chrono::duration<double, R> &s) {
    serialize<W>(wm, s.count());
}

template <cluster_version_t W, typename R>
MUST_USE archive_result_t deserialize(read_stream_t *s, chrono::duration<double, R> *p) {
    double d;
    archive_result_t res = deserialize<W>(s, &d);
    if (bad(res)) { return res; }
    *p = chrono::duration<double, R>{d};
    return res;
}

template <cluster_version_t W, typename R>
void serialize(write_message_t *wm, const chrono::duration<int64_t, R> &s) {
    serialize<W>(wm, s.count());
}

template <cluster_version_t W, typename R>
MUST_USE archive_result_t deserialize(read_stream_t *s, chrono::duration<int64_t, R> *p) {
    int64_t d;
    archive_result_t res = deserialize<W>(s, &d);
    if (bad(res)) { return res; }
    *p = chrono::duration<int64_t, R>{d};
    return res;
}


inline chrono::nanoseconds remaining_nanos(realtime_t t) {
    return t.time_since_epoch() - std::chrono::duration_cast<chrono::seconds>(t.time_since_epoch()) ;
}

inline chrono::nanoseconds remaining_nanos(chrono::nanoseconds t) {
    return t - chrono::duration_cast<chrono::seconds>(t) ;
}

inline ticks_t get_ticks() {
    return chrono::steady_clock::now().time_since_epoch();
}

template <typename T, typename A>
inline T tick_floor(A t)
{
    return chrono::duration_cast<T>(t);
}

template<typename T, typename P, typename R>
inline constexpr T time_cast(std::chrono::duration<P,R> ticks)
{
   return chrono::duration_cast<T>(ticks);
}


template <typename T>
T from_datum_time(datum_micro_t ticks)
{
    return chrono::duration_cast<T>(ticks);
}

template <typename T>
T to_datum_time(ticks_t ticks)
{
    return chrono::duration_cast<T>(ticks);
}


#endif  // TIME_HPP_

#ifndef TIME_HPP_
#define TIME_HPP_

#include <stdint.h>
#include <time.h>

#include <chrono>

#include "version.hpp"
#include "containers/archive/archive.hpp"



using monotonic_t = std::chrono::steady_clock::time_point;
using realtime_t = std::chrono::system_clock::time_point;

inline auto clock_monotonic()
{
    return std::chrono::steady_clock::now();
}

inline auto clock_realtime()
{
    return std::chrono::system_clock::now();
}

inline time_t clock_to_time(realtime_t t)
{
    return std::chrono::system_clock::to_time_t(t);
}

inline realtime_t time_to_clock(time_t t)
{
    return std::chrono::system_clock::from_time_t(t);
}

/*
template <cluster_version_t W>
void serialize(write_message_t *wm, const realtime_t &s) {
    serialize<W>(wm,  std::chrono::duration_cast<std::chrono::seconds>(s.time_since_epoch()));
}

template <cluster_version_t W>
MUST_USE archive_result_t deserialize(read_stream_t *s, realtime_t *p) {
    std::chrono::seconds d;
    archive_result_t res = deserialize<W>(s, &d);
    if (bad(res)) { return res; }
    *p = realtime_t{std::chrono::seconds{d}};
    return res;
}

template <cluster_version_t W>
void serialize(write_message_t *wm, const monotonic_t &s) {
    serialize<W>(wm,  std::chrono::duration_cast<std::chrono::seconds>(s.time_since_epoch()));
}

template <cluster_version_t W>
MUST_USE archive_result_t deserialize(read_stream_t *s, monotonic_t *p) {
    std::chrono::seconds d;
    archive_result_t res = deserialize<W>(s, &d);
    if (bad(res)) { return res; }
    *p = monotonic_t{std::chrono::seconds{d}};
    return res;
}

*/

using microticks_t = std::chrono::duration<int64_t, std::micro>;
using ticks_t = std::chrono::duration<int64_t, std::nano>;

namespace chrono {
	using namespace std::chrono;

	using hours  = std::chrono::duration<int64_t, std::ratio<3600>>;
	using days = std::chrono::duration<int64_t, std::ratio<86400>>;
}

using datum_seconds_t = std::chrono::duration<double>;
using datum_milli_t = std::chrono::duration<double, std::milli>;
using datum_micro_t = std::chrono::duration<double, std::micro>;

template <class To, class Rep, class Period>
constexpr To ceil(const std::chrono::duration<Rep, Period>& d)
{
    To t = std::chrono::duration_cast<To>(d);
    if (t < d)
        return t + To{1};
    return t;
}


template <cluster_version_t W, typename R>
void serialize(write_message_t *wm, const std::chrono::duration<double, R> &s) {
    serialize<W>(wm, s.count());
}

template <cluster_version_t W, typename R>
MUST_USE archive_result_t deserialize(read_stream_t *s, std::chrono::duration<double, R> *p) {
    double d;
    archive_result_t res = deserialize<W>(s, &d);
    if (bad(res)) { return res; }
    *p = std::chrono::duration<double, R>{d};
    return res;
}

template <cluster_version_t W, typename R>
void serialize(write_message_t *wm, const std::chrono::duration<int64_t, R> &s) {
    serialize<W>(wm, s.count());
}

template <cluster_version_t W, typename R>
MUST_USE archive_result_t deserialize(read_stream_t *s, std::chrono::duration<int64_t, R> *p) {
    int64_t d;
    archive_result_t res = deserialize<W>(s, &d);
    if (bad(res)) { return res; }
    *p = std::chrono::duration<int64_t, R>{d};
    return res;
}

inline ticks_t remaining_nanos(realtime_t t) {
    return t.time_since_epoch() - std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()) ;
}

inline ticks_t get_ticks() {
    return std::chrono::steady_clock::now().time_since_epoch();
}

template <typename T, typename A>
inline auto tick_floor(A t)
{
    return std::chrono::duration_cast<T>(t);
}

template<typename T, typename P, typename R>
inline constexpr auto time_cast(std::chrono::duration<P,R> ticks)
{
   return std::chrono::duration_cast<T>(ticks);
}


template <typename T>
T from_datum_time(datum_micro_t ticks)
{
    return std::chrono::duration_cast<T>(ticks);
}

template <typename T>
T to_datum_time(ticks_t ticks)
{
    return std::chrono::duration_cast<T>(ticks);
}


#endif  // TIME_HPP_

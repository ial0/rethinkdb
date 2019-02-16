// Copyright 2010-2014 RethinkDB, all rights reserved.
#ifndef RPC_SEMILATTICE_JOINS_VERSIONED_HPP_
#define RPC_SEMILATTICE_JOINS_VERSIONED_HPP_

#include <algorithm>
#include <limits>

#include "arch/runtime/runtime_utils.hpp"
#include "containers/archive/optional.hpp"
#include "containers/uuid.hpp"
#include "rpc/serialize_macros.hpp"
#include "time.hpp"

/* A `versioned_t` is used in the semilattices to track a setting that the user is
allowed to update. If the setting is updated in two places simultaneously, the
semilattice join will pick the one that came later as measured by the servers' clocks.
*/

template<class T>
class versioned_t {
public:
    versioned_t() :
        timestamp(std::chrono::system_clock::to_time_t(realtime_t::min())),
        /* We only use `nil_uuid()` in this default constructor, so if two `versioned_t`s
        have `nil_uuid()` as their tiebreaker they must both have the default value
        anyway; so we don't need a unique tiebreaker. The advantage of `nil_uuid()` is
        that it makes the default constructor deterministic. */
        tiebreaker(nil_uuid()) { }

    /* This constructor should only be used when a new entry in the semilattices is first
    being initialized. In particular, DO NOT do the following:
        versioned_t<T> metadata = semilattice_view->get();
        T old_value = metadata.get_ref();
        T new_value = f(old_value);
        metadata = versioned_t<T>(new_value);   <-- WRONG!
        semilattice_view->join(metadata);
    Because this constructor does not initialize the timestamp to the current time,
    `new_value` will be overridden by `old_value`, so the above code block will have no
    effect. Instead, you should use the `set()` or `apply_write()` method to update the
    `versioned_t`. */
    explicit versioned_t(const T &initial_value) :
        value(initial_value),
        /* Using the minimum timestamp ensures that this `versioned_t` will always be
        overwritten by one generated by `set()` or `apply_write()` */
        timestamp(std::chrono::system_clock::to_time_t(realtime_t::min())),
        /* We generate a tiebreaker so that if we join two `versioned_t`s generated with
        this constructor together, the join will produce a deterministic result instead
        of depending on the order in which they are joined. */
        tiebreaker(generate_uuid()) { }

    /* This constructor is only used when migrating from pre-v1.16 metadata files that
    used vector clocks */
    static versioned_t make_with_manual_timestamp(time_t time, const T &value) {
        versioned_t v;
        v.timestamp = time;
        v.tiebreaker = generate_uuid();
        v.value = value;
        return v;
    }

    // This getter is only used when migrating from v1.16 metadata to v2.1
    const time_t &get_timestamp() const { return timestamp; }
    const uuid_u &get_tiebreaker() const { return tiebreaker; }

    const T &get_ref() const {
        return value;
    }

    void set(const T &new_value) {
        value = new_value;
        on_change();
    }

    template<class callable_t>
    void apply_write(const callable_t &function) {
        ASSERT_FINITE_CORO_WAITING;
        function(&value);
        on_change();
    }

    RDB_MAKE_ME_SERIALIZABLE_3(versioned_t, value, timestamp, tiebreaker);

private:
    template<class TT>
    friend bool operator==(const versioned_t<TT> &a, const versioned_t<TT> &b);

    template<class TT>
    friend void semilattice_join(versioned_t<TT> *a, const versioned_t<TT> &b);

    void on_change() {
        /* Ordinarily, this will set `timestamp` to `time(NULL)`. However, if we make
        multiple updates in a single second, it's important that we always set
        `timestamp` to a higher value than it was before; hence the `std::max`. This is
        also important in the case where servers' clocks aren't sane; if one server sets
        `timestamp` to a time far in the future, this logic will still allow the other
        servers to change the stored value. */
        timestamp = std::max(std::chrono::system_clock::to_time_t(clock_realtime())+1, time(nullptr));

        tiebreaker = generate_uuid();
    }

    T value;
    time_t timestamp;
    uuid_u tiebreaker;
};

template <class T>
bool operator==(const versioned_t<T> &a, const versioned_t<T> &b) {
    return (a.value == b.value) && \
        (a.timestamp == b.timestamp) && \
        (a.tiebreaker == b.tiebreaker);
}

template <class T>
void semilattice_join(versioned_t<T> *a, const versioned_t<T> &b) {
    if (a->timestamp < b.timestamp ||
            (a->timestamp == b.timestamp && a->tiebreaker < b.tiebreaker)) {
        *a = b;
    }
}

#endif /* RPC_SEMILATTICE_JOINS_VERSIONED_HPP_ */


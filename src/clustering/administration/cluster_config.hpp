// Copyright 2010-2015 RethinkDB, all rights reserved.
#ifndef CLUSTERING_ADMINISTRATION_CLUSTER_CONFIG_HPP_
#define CLUSTERING_ADMINISTRATION_CLUSTER_CONFIG_HPP_

#include <memory>
#include <string>
#include <vector>

#include "rdb_protocol/artificial_table/caching_cfeed_backend.hpp"
#include "rpc/semilattice/view.hpp"

/* The `rethinkdb.cluster_config` table is a catch-all for settings that don't fit
elsewhere but aren't complicated enough to deserve their own table. It has a fixed set of
rows, each of which has a unique format and corresponds to a different setting.

Right now the only row is `heartbeat`, with the format
`{"id": "heartbeat", "heartbeat_timeout_secs": ...}`. */

class cluster_config_artificial_table_backend_t :
    public caching_cfeed_artificial_table_backend_t
{
public:
    cluster_config_artificial_table_backend_t(
            rdb_context_t *rdb_context,
            lifetime_t<name_resolver_t const &> name_resolver,
            std::shared_ptr<semilattice_readwrite_view_t<
                heartbeat_semilattice_metadata_t> > _heartbeat_sl_view);
    ~cluster_config_artificial_table_backend_t();

    std::string get_primary_key_name();

    bool read_all_rows_as_vector(
            auth::user_context_t const &user_context,
            signal_t *interruptor,
            std::vector<ql::datum_t> *rows_out,
            admin_err_t *error_out);

    bool read_row(
            auth::user_context_t const &user_context,
            ql::datum_t primary_key,
            signal_t *interruptor,
            ql::datum_t *row_out,
            admin_err_t *error_out);

    bool write_row(
            auth::user_context_t const &user_context,
            ql::datum_t primary_key,
            bool pkey_was_autogenerated,
            ql::datum_t *new_value_inout,
            signal_t *interruptor,
            admin_err_t *error_out);

    void set_notifications(bool);

private:
    /* The abstract class `doc_t` represents a row in the `cluster_config` table. In the
    future it will have more subclasses. We might eventually want to move it and its
    subclasses out of this file. */
    class doc_t {
    public:
        /* Computes the current value of the row, including the primary key. */
        virtual bool read(
                signal_t *interruptor,
                ql::datum_t *row_out,
                admin_err_t *error_out) = 0;
        /* Applies a change to the row. */
        virtual bool write(
                signal_t *interruptor,
                ql::datum_t *value_inout,
                admin_err_t *error_out) = 0;
        /* Registers or deregisters a function that should be called if the row ever
        changes. */
        virtual void set_notification_callback(const std::function<void()> &) = 0;
    protected:
        virtual ~doc_t() { }   // make compiler happy
    };

    class heartbeat_doc_t : public doc_t {
    public:
        explicit heartbeat_doc_t(std::shared_ptr<semilattice_readwrite_view_t<
            heartbeat_semilattice_metadata_t> > _sl_view) : sl_view(_sl_view) { }
        bool read(
                signal_t *interruptor,
                ql::datum_t *row_out,
                admin_err_t *error_out);
        bool write(
                signal_t *interruptor,
                ql::datum_t *row_out,
                admin_err_t *error_out);
        void set_notification_callback(const std::function<void()> &fun);
    private:
        std::shared_ptr<
            semilattice_readwrite_view_t<heartbeat_semilattice_metadata_t> > sl_view;
        scoped_ptr_t<
                semilattice_read_view_t<heartbeat_semilattice_metadata_t>::subscription_t
            > subs;
    };

    heartbeat_doc_t heartbeat_doc;

    std::map<std::string, doc_t *> docs;
};

#endif /* CLUSTERING_ADMINISTRATION_CLUSTER_CONFIG_HPP_ */


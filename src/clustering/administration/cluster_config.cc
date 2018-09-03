// Copyright 2010-2015 RethinkDB, all rights reserved.
#include "clustering/administration/cluster_config.hpp"

#include "clustering/administration/admin_op_exc.hpp"
#include "clustering/administration/datum_adapter.hpp"

cluster_config_artificial_table_backend_t::cluster_config_artificial_table_backend_t(
        rdb_context_t *rdb_context,
        lifetime_t<name_resolver_t const &> name_resolver,
        std::shared_ptr<semilattice_readwrite_view_t<
            heartbeat_semilattice_metadata_t> > _heartbeat_sl_view)
    : caching_cfeed_artificial_table_backend_t(
        name_string_t::guarantee_valid("cluster_config"), rdb_context, name_resolver),
      heartbeat_doc(_heartbeat_sl_view) {
    docs["heartbeat"] = &heartbeat_doc;
}

cluster_config_artificial_table_backend_t::~cluster_config_artificial_table_backend_t() {
    begin_changefeed_destruction();
}

std::string cluster_config_artificial_table_backend_t::get_primary_key_name() {
    return "id";
}

bool cluster_config_artificial_table_backend_t::read_all_rows_as_vector(
        UNUSED auth::user_context_t const &user_context,
        signal_t *interruptor,
        std::vector<ql::datum_t> *rows_out,
        admin_err_t *error_out) {
    rows_out->clear();

    for (auto it = docs.begin(); it != docs.end(); ++it) {
        ql::datum_t row;
        if (!it->second->read(interruptor, &row, error_out)) {
            return false;
        }
        rows_out->push_back(row);
    }
    return true;
}

bool cluster_config_artificial_table_backend_t::read_row(
        UNUSED auth::user_context_t const &user_context,
        ql::datum_t primary_key,
        signal_t *interruptor,
        ql::datum_t *row_out,
        admin_err_t *error_out) {
    if (primary_key.get_type() != ql::datum_t::R_STR) {
        *row_out = ql::datum_t();
        return true;
    }
    auto it = docs.find(primary_key.as_str().to_std());
    if (it == docs.end()) {
        *row_out = ql::datum_t();
        return true;
    }
    return it->second->read(interruptor, row_out, error_out);
}

bool cluster_config_artificial_table_backend_t::write_row(
        UNUSED auth::user_context_t const &user_context,
        ql::datum_t primary_key,
        UNUSED bool pkey_was_autogenerated,
        ql::datum_t *new_value_inout,
        signal_t *interruptor,
        admin_err_t *error_out) {
    if (!new_value_inout->has()) {
        *error_out = admin_err_t{
            "It's illegal to delete rows from the `rethinkdb.cluster_config` table.",
            query_state_t::FAILED};
        return false;
    }
    const char *missing_message = "It's illegal to insert new rows into the "
        "`rethinkdb.cluster_config` table.";
    if (primary_key.get_type() != ql::datum_t::R_STR) {
        *error_out = admin_err_t{missing_message, query_state_t::FAILED};
        return false;
    }
    auto it = docs.find(primary_key.as_str().to_std());
    if (it == docs.end()) {
        *error_out = admin_err_t{missing_message, query_state_t::FAILED};
        return false;
    }
    return it->second->write(interruptor, new_value_inout, error_out);
}

void cluster_config_artificial_table_backend_t::set_notifications(bool should_notify) {
    /* Note that we aren't actually modifying the `docs` map itself, just the objects
    that it points at. So this could have been `const auto &pair`, but that might be
    misleading. */
    for (auto &&pair : docs) {
        if (should_notify) {
            std::string name = pair.first;
            pair.second->set_notification_callback(
                [this, name]() {
                    notify_row(ql::datum_t(datum_string_t(name)));
                });
        } else {
            pair.second->set_notification_callback(nullptr);
        }
    }
}

bool cluster_config_artificial_table_backend_t::heartbeat_doc_t::read(
        UNUSED signal_t *interruptor,
        ql::datum_t *row_out,
        UNUSED admin_err_t *error_out) {
    on_thread_t thread_switcher(sl_view->home_thread());
    ql::datum_object_builder_t obj_builder;
    obj_builder.overwrite("id", ql::datum_t("heartbeat"));
    obj_builder.overwrite("heartbeat_timeout_secs", ql::datum_t(to_datum_time<datum_seconds_t>(
        sl_view->get().heartbeat_timeout.get_ref()).count()));
    *row_out = std::move(obj_builder).to_datum();
    return true;
}

bool cluster_config_artificial_table_backend_t::heartbeat_doc_t::write(
        UNUSED signal_t *interruptor,
        ql::datum_t *row_inout,
        admin_err_t *error_out) {
    converter_from_datum_object_t converter;
    admin_err_t dummy_error;
    if (!converter.init(*row_inout, &dummy_error)) {
        crash("artificial_table_t should guarantee input is an object");
    }
    ql::datum_t dummy_pkey;
    if (!converter.get("id", &dummy_pkey, &dummy_error)) {
        crash("artificial_table_t should guarantee primary key is present and correct");
    }

    ql::datum_t heartbeat_timeout_datum;
    if (!converter.get("heartbeat_timeout_secs", &heartbeat_timeout_datum, error_out)) {
        return false;
    }
    datum_seconds_t heartbeat_timeout;
    if (heartbeat_timeout_datum.get_type() == ql::datum_t::R_NUM) {
        heartbeat_timeout = datum_seconds_t{heartbeat_timeout_datum.as_num()};
        if (heartbeat_timeout < datum_seconds_t{2}) {
            *error_out = admin_err_t{
                "The heartbeat timeout must be at least two seconds",
                query_state_t::FAILED};
            return false;
        }
    } else {
        *error_out = admin_err_t{
            "Expected a number; got " + heartbeat_timeout_datum.print(),
            query_state_t::FAILED};
        return false;
    }

    if (!converter.check_no_extra_keys(error_out)) {
        return false;
    }

    {
        on_thread_t thread_switcher(sl_view->home_thread());
        heartbeat_semilattice_metadata_t metadata = sl_view->get();
        metadata.heartbeat_timeout.set(from_datum_time<milli_t>(heartbeat_timeout));
        sl_view->join(metadata);
    }

    return true;
}

void
cluster_config_artificial_table_backend_t::heartbeat_doc_t::set_notification_callback(
        const std::function<void()> &fun) {
    if (static_cast<bool>(fun)) {
        subs = make_scoped<semilattice_read_view_t<
            heartbeat_semilattice_metadata_t>::subscription_t>(fun, sl_view);
    } else {
        subs.reset();
    }
}

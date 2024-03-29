//
// Created by kilingzhang on 2021/7/10.
//

#include "php_opentelemetry.h"
#include "include/zend_hook_pdo.h"
#include "include/utils.h"
#include "zend_types.h"
#include "ext/pdo/php_pdo_driver.h"

#include <regex>
#include <cstdlib>

#include "opentelemetry/proto/trace/v1/trace.pb.h"

using namespace opentelemetry::proto::trace::v1;

std::vector<std::string> pdoKeysCommands;

void opentelemetry_pdo_handler(INTERNAL_FUNCTION_PARAMETERS) {
    zend_function *zf = execute_data->func;
    std::string name;
    std::string class_name;
    std::string function_name;
    std::string code_stacktrace;

    if (zf->internal_function.scope != nullptr && zf->internal_function.scope->name != nullptr) {
        class_name = std::string(ZSTR_VAL(zf->internal_function.scope->name));
    }
    if (zf->internal_function.function_name != nullptr) {
        function_name = std::string(ZSTR_VAL(zf->internal_function.function_name));
    }
    name = find_trace_add_scope_name(
        zf->internal_function.function_name, zf->internal_function.scope, zf->internal_function.fn_flags);

    std::string cmd = function_name;
    std::transform(function_name.begin(), function_name.end(), cmd.begin(), ::tolower);
    Span *span;
    if (is_has_provider()) {
        std::string parentId = OPENTELEMETRY_G(provider)->latestSpan().span_id();
        span = OPENTELEMETRY_G(provider)->createSpan(name, Span_SpanKind_SPAN_KIND_CLIENT);
        span->set_parent_span_id(parentId);
        zend_execute_data *caller = execute_data->prev_execute_data;
        if (caller != nullptr && caller->func) {
            code_stacktrace = find_code_stacktrace(caller);
            if (!code_stacktrace.empty()) {
                set_string_attribute(span->add_attributes(), "code.stacktrace", code_stacktrace);
            }
        }

        std::string source;
        uint32_t arg_count = ZEND_CALL_NUM_ARGS(execute_data);
        if (arg_count) {
            zval *p = ZEND_CALL_ARG(execute_data, 1);
            // db.statement
            switch (Z_TYPE_P(p)) {
            case IS_STRING:
                if (function_name != "__construct") {
                    set_string_attribute(span->add_attributes(), "db.statement", Z_STRVAL_P(p));
                } else {
                    source = Z_STRVAL_P(p);
                    // username
                    p = ZEND_CALL_ARG(execute_data, 2);
                    set_string_attribute(span->add_attributes(), "db.user", Z_STRVAL_P(p));
                }
                break;
            }
        }

        if (function_name == "__construct") {
            set_string_attribute(span->add_attributes(), "db.operation", "connect");
        } else if (function_name == "begintransaction") {
            set_string_attribute(span->add_attributes(), "db.operation", "begin");
        } else if (function_name == "rollback") {
            set_string_attribute(span->add_attributes(), "db.operation", "rollback");
        } else if (function_name == "commit") {
            set_string_attribute(span->add_attributes(), "db.operation", "commit");
        }

        char db_type[64] = {0};
        pdo_dbh_t *dbh = Z_PDO_DBH_P(&(execute_data->This));
        bool is_set_db_system = false;
        if (dbh != nullptr) {
            if (dbh->driver != nullptr && dbh->driver->driver_name != nullptr) {
                memcpy(db_type, (char *) dbh->driver->driver_name, dbh->driver->driver_name_len);
                is_set_db_system = true;
                set_string_attribute(span->add_attributes(), "db.system", db_type);
            }

            if (dbh->username != nullptr) {
                set_string_attribute(span->add_attributes(), "db.user", dbh->username);
            }

            if (dbh->data_source != nullptr && db_type[0] != '\0') {
                source = dbh->data_source;
            }

            if (!source.empty()) {
                set_string_attribute(span->add_attributes(), "db.connection_string", source);

                std::regex ws_re(";");
                std::regex kv_re("=");
                std::vector<std::string> items(std::sregex_token_iterator(source.begin(), source.end(), ws_re, -1),
                                               std::sregex_token_iterator());

                for (auto item : items) {
                    std::vector<std::string> kv(std::sregex_token_iterator(item.begin(), item.end(), kv_re, -1),
                                                std::sregex_token_iterator());
                    if (kv.size() >= 2) {
                        if (kv[0] == "host" || kv[0] == "mysql:host") {
                            if (inet_addr(kv[1].c_str()) != INADDR_NONE) {
                                set_string_attribute(span->add_attributes(), "net.peer.ip", kv[1]);
                            } else {
                                set_string_attribute(span->add_attributes(), "net.peer.name", kv[1]);
                            }
                        } else if (kv[0] == "port" || kv[0] == "mysql:port") {
                            set_int64_attribute(
                                span->add_attributes(), "net.peer.port", strtol(kv[1].c_str(), nullptr, 10));
                        } else if (kv[0] == "dbname" || kv[0] == "mysql:dbname") {
                            set_string_attribute(span->add_attributes(), "db.name", kv[1]);
                        }
                    }
                }

                set_string_attribute(span->add_attributes(), "net.transport", "IP.TCP");
            }
        }

        if (!is_set_db_system) {
            set_string_attribute(span->add_attributes(), "db.system", COMPONENTS_MYSQL);
        }
    }
    if (!opentelemetry_pdo_original_handler_map.empty() &&
        opentelemetry_pdo_original_handler_map.find(cmd) != opentelemetry_pdo_original_handler_map.end()) {
        opentelemetry_pdo_original_handler_map[cmd](INTERNAL_FUNCTION_PARAM_PASSTHRU);
    }

    if (is_has_provider()) {
        pdo_dbh_t *dbh = Z_PDO_DBH_P(&(execute_data->This));
        if (dbh != nullptr &&
            (!is_equal_const(dbh->error_code, PDO_ERR_NONE) ||
             (dbh->query_stmt != nullptr && !is_equal_const(dbh->query_stmt->error_code, PDO_ERR_NONE)))) {
            pdo_error_type *pdo_err = nullptr;
            char *supp = nullptr;
            zend_long native_code = 0;
            zend_string *message = nullptr;
            zval info;
            ZVAL_UNDEF(&info);
            if (!is_equal_const(dbh->error_code, PDO_ERR_NONE)) {
                pdo_err = &dbh->error_code;
            } else if (!is_equal_const(dbh->query_stmt->error_code, PDO_ERR_NONE)) {
                pdo_err = &dbh->query_stmt->error_code;
            }
            if (dbh->methods->fetch_err) {
                array_init(&info);
                add_next_index_string(&info, *pdo_err);
                if (dbh->methods->fetch_err(dbh, dbh->query_stmt, &info)) {
                    zval *item;
                    if ((item = zend_hash_index_find(Z_ARRVAL(info), 1)) != nullptr) {
                        native_code = Z_LVAL_P(item);
                    }
                    if ((item = zend_hash_index_find(Z_ARRVAL(info), 2)) != nullptr) {
                        supp = estrndup(Z_STRVAL_P(item), Z_STRLEN_P(item));
                    }
                }
            }
            if (supp) {
                message = strpprintf(0, "SQLSTATE[%s] [" ZEND_LONG_FMT "] %s", *pdo_err, native_code, supp);
            }

            if (message) {
                Provider::errorEnd(span, std::string(message->val));
                zend_string_release(message);
            } else {
                Provider::okEnd(span);
            }

            if (!Z_ISUNDEF(info)) {
                zval_ptr_dtor(&info);
            }
            if (supp) {
                efree(supp);
            }
        } else {
            Provider::okEnd(span);
        }
    }

    cmd.clear();
    cmd.shrink_to_fit();
    name.shrink_to_fit();
    class_name.shrink_to_fit();
    function_name.shrink_to_fit();
    code_stacktrace.shrink_to_fit();
}

void register_zend_hook_pdo() {
    pdoKeysCommands = {"__construct", "begintransaction", "rollback", "commit", "exec", "query"};
    zend_class_entry *old_class;
    zend_function *old_function;
    if ((old_class = OPENTELEMETRY_OLD_CN("pdo")) != nullptr) {
        for (const auto &item : pdoKeysCommands) {
            if ((old_function = OPENTELEMETRY_OLD_FN_TABLE(&old_class->function_table, item.c_str())) != nullptr) {
                opentelemetry_pdo_original_handler_map[item.c_str()] = old_function->internal_function.handler;
                old_function->internal_function.handler = opentelemetry_pdo_handler;
            }
        }
    }
}

void unregister_zend_hook_pdo() {
    zend_class_entry *old_class;
    zend_function *old_function;
    if ((old_class = OPENTELEMETRY_OLD_CN("pdo")) != nullptr) {
        for (const auto &item : pdoKeysCommands) {
            if ((old_function = OPENTELEMETRY_OLD_FN_TABLE(&old_class->function_table, item.c_str())) != nullptr) {
                old_function->internal_function.handler = opentelemetry_pdo_original_handler_map[item.c_str()];
            }
        }
    }
    pdoKeysCommands.clear();
}
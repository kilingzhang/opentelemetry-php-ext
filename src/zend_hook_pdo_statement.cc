//
// Created by kilingzhang on 2021/7/10.
//

#include "php_opentelemetry.h"
#include "include/zend_hook_pdo_statement.h"
#include "include/utils.h"
#include "zend_types.h"
#include "ext/pdo/php_pdo_driver.h"

#include <regex>
#include <cstdlib>

#include "opentelemetry/proto/trace/v1/trace.pb.h"

using namespace opentelemetry::proto::trace::v1;

std::vector<std::string> pdoStmKeysCommands;

void opentelemetry_pdo_statement_handler(INTERNAL_FUNCTION_PARAMETERS) {
  zend_function *zf = execute_data->func;
  std::string name;
  std::string class_name;
  std::string function_name;
  std::string trace_caller;

  if (zf->internal_function.scope != nullptr && zf->internal_function.scope->name != nullptr) {
    class_name = std::string(ZSTR_VAL(zf->internal_function.scope->name));
  }
  if (zf->internal_function.function_name != nullptr) {
    function_name = std::string(ZSTR_VAL(zf->internal_function.function_name));
  }
  name = find_trace_add_scope_name(zf->internal_function.function_name, zf->internal_function.scope,
                                   zf->internal_function.fn_flags);

  std::string cmd = function_name;
  std::transform(function_name.begin(), function_name.end(), cmd.begin(), ::tolower);
  Span *span;
  if (is_has_provider()) {
    std::string parentId = OPENTELEMETRY_G(provider)->latestSpan().span_id();
    span = OPENTELEMETRY_G(provider)->createSpan(name, Span_SpanKind_SPAN_KIND_INTERNAL);
    set_string_attribute(span->add_attributes(), COMPONENTS_KEY, COMPONENTS_MYSQL);
    span->set_parent_span_id(parentId);
    zend_execute_data *caller = execute_data->prev_execute_data;
    if (caller != nullptr && caller->func) {
      trace_caller = find_trace_caller(caller);
      if (!trace_caller.empty()) {
        set_string_attribute(span->add_attributes(), "trace.caller", trace_caller);
      }
    }

    char db_type[64] = {0};
    auto *stmt = (pdo_stmt_t *) Z_PDO_STMT_P(&(execute_data->This));
    if (stmt != nullptr) {
      set_string_attribute(span->add_attributes(), "db.statement", stmt->query_string);
      if (stmt->dbh != nullptr && stmt->dbh->driver->driver_name != nullptr) {
        memcpy(db_type, (char *) stmt->dbh->driver->driver_name, stmt->dbh->driver->driver_name_len);
        set_string_attribute(span->add_attributes(), "db.type", db_type);
      }

      if (db_type[0] != '\0' && stmt->dbh != nullptr && stmt->dbh->data_source != nullptr) {
        std::string source = stmt->dbh->data_source;

        set_string_attribute(span->add_attributes(), "db.data_source", source);

        std::regex ws_re(";");
        std::regex kv_re("=");
        std::vector<std::string> items(std::sregex_token_iterator(source.begin(), source.end(), ws_re, -1),
                                       std::sregex_token_iterator());

        std::string host;
        std::string port;

        for (auto item:items) {
          std::vector<std::string> kv(std::sregex_token_iterator(item.begin(), item.end(), kv_re, -1),
                                      std::sregex_token_iterator());
          if (kv.size() >= 2) {
            if (kv[0] == "host") {
              host = kv[1];
            }
            if (kv[0] == "port") {
              port = kv[1];
            }
          }
        }

        set_string_attribute(span->add_attributes(), "db.host", host);
        set_int64_attribute(span->add_attributes(), "db.port", strtol(port.c_str(), nullptr, 10));
        set_string_attribute(span->add_attributes(), "db.peer", host + ":" + port);
      }
    }

  }
  if (!opentelemetry_pdo_statement_original_handler_map.empty() &&
      opentelemetry_pdo_statement_original_handler_map.find(cmd) !=
          opentelemetry_pdo_statement_original_handler_map.end()) {
    opentelemetry_pdo_statement_original_handler_map[cmd](INTERNAL_FUNCTION_PARAM_PASSTHRU);
  }

  if (is_has_provider()) {
    okEnd(span);
  }

  cmd.clear();
  cmd.shrink_to_fit();
  name.shrink_to_fit();
  class_name.shrink_to_fit();
  function_name.shrink_to_fit();
  trace_caller.shrink_to_fit();
}

void register_zend_hook_pdo_statement() {
  pdoStmKeysCommands = {"bindcolumn", "bindparam", "bindvalue", "closecursor", "columncount", "debugdumpparams",
      "errorcode", "errorinfo", "execute", "fetch", "fetchall", "fetchcolumn", "fetchobject",
      "getattribute", "getcolumnMeta", "nextrowset", "rowcount", "setattribute", "setfetchmode"};
  zend_class_entry *old_class;
  zend_function *old_function;
  if ((old_class = OPENTELEMETRY_OLD_CN("pdostatement")) != nullptr) {
    for (const auto &item : pdoStmKeysCommands) {
      if ((old_function = OPENTELEMETRY_OLD_FN_TABLE(
          &old_class->function_table, item.c_str())) !=
          nullptr) {
        opentelemetry_pdo_statement_original_handler_map[item.c_str()] = old_function->internal_function.handler;
        old_function->internal_function.handler = opentelemetry_pdo_statement_handler;
      }
    }
  }
}

void unregister_zend_hook_pdo_statement() {
  zend_class_entry *old_class;
  zend_function *old_function;
  if ((old_class = OPENTELEMETRY_OLD_CN("pdostatement")) != nullptr) {
    for (const auto &item : pdoStmKeysCommands) {
      if ((old_function = OPENTELEMETRY_OLD_FN_TABLE(
          &old_class->function_table, item.c_str())) !=
          nullptr) {
        old_function->internal_function.handler = opentelemetry_pdo_statement_original_handler_map[item.c_str()];
      }
    }
  }
}
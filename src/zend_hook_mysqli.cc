//
// Created by kilingzhang on 2021/7/10.
//

#include "php_opentelemetry.h"
#include "include/zend_hook_mysqli.h"
#include "include/utils.h"
#include "zend_types.h"
#include "ext/pdo/php_pdo_driver.h"

#include "opentelemetry/proto/trace/v1/trace.pb.h"

using namespace opentelemetry::proto::trace::v1;

#ifdef MYSQLI_USE_MYSQLND

#include "ext/mysqli/php_mysqli_structs.h"

#endif

std::vector<std::string> mysqliKeysCommands;

void opentelemetry_mysqli_handler(INTERNAL_FUNCTION_PARAMETERS) {
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
  name = find_trace_add_scope_name(zf->internal_function.function_name, zf->internal_function.scope,
                                   zf->internal_function.fn_flags);

  std::string cmd = function_name;
  std::transform(function_name.begin(), function_name.end(), cmd.begin(), ::tolower);
  Span *span;
  if (is_has_provider()) {

    std::string parentId = OPENTELEMETRY_G(provider)->latestSpan().span_id();
    span = OPENTELEMETRY_G(provider)->createSpan(name, Span_SpanKind_SPAN_KIND_CLIENT);
    set_string_attribute(span->add_attributes(), COMPONENTS_KEY, COMPONENTS_DB);
    set_string_attribute(span->add_attributes(), "db.system", COMPONENTS_MYSQL);
    span->set_parent_span_id(parentId);
    zend_execute_data *caller = execute_data->prev_execute_data;
    if (caller != nullptr && caller->func) {
      code_stacktrace = find_code_stacktrace(caller);
      if (!code_stacktrace.empty()) {
        set_string_attribute(span->add_attributes(), "code.stacktrace", code_stacktrace);
      }
    }

#ifdef MYSQLI_USE_MYSQLND
    mysqli_object *mysqli = nullptr;
#endif
    uint32_t arg_count = ZEND_CALL_NUM_ARGS(execute_data);
    zval *statement = nullptr;
    if (class_name == "mysqli") {
      if (arg_count) {
        statement = ZEND_CALL_ARG(execute_data, 1);
      }
#ifdef MYSQLI_USE_MYSQLND
      mysqli = (mysqli_object *) Z_MYSQLI_P(&(execute_data->This));
#endif
    } else { //is procedural
      if (arg_count > 1) {
        statement = ZEND_CALL_ARG(execute_data, 2);
      }
#ifdef MYSQLI_USE_MYSQLND
      zval *obj = ZEND_CALL_ARG(execute_data, 1);
      if (Z_TYPE_P(obj) != IS_NULL) {
        mysqli = (mysqli_object *) Z_MYSQLI_P(obj);
      }
#endif
    }

    if (statement != nullptr && Z_TYPE_P(statement) == IS_STRING) {
      set_string_attribute(span->add_attributes(), "db.statement", Z_STRVAL_P(statement));
    }

#ifdef MYSQLI_USE_MYSQLND
    if (mysqli != nullptr) {
      auto *my_res = (MYSQLI_RESOURCE *) mysqli->ptr;
      if (my_res && my_res->ptr) {
        auto *mysql = (MY_MYSQL *) my_res->ptr;
        if (mysql->mysql) {
#if PHP_VERSION_ID >= 70100
          std::string host = mysql->mysql->data->hostname.s;
#else
          std::string host = mysql->mysql->data->host;
#endif

          set_string_attribute(span->add_attributes(), "net.transport", "IP.TCP");

          if (inet_addr(host.c_str()) != INADDR_NONE) {
            set_string_attribute(span->add_attributes(), "net.peer.ip", host);
          } else {
            set_string_attribute(span->add_attributes(), "net.peer.name", host);
          }
          set_int64_attribute(span->add_attributes(), "net.peer.port", mysql->mysql->data->port);

        }
      }
    }
#endif

  }
  if (!opentelemetry_mysqli_original_handler_map.empty() &&
      opentelemetry_mysqli_original_handler_map.find(cmd) !=
          opentelemetry_mysqli_original_handler_map.end()) {
    opentelemetry_mysqli_original_handler_map[cmd](INTERNAL_FUNCTION_PARAM_PASSTHRU);
  }

  if (is_has_provider()) {
    okEnd(span);
  }

  cmd.clear();
  cmd.shrink_to_fit();
  name.shrink_to_fit();
  class_name.shrink_to_fit();
  function_name.shrink_to_fit();
  code_stacktrace.shrink_to_fit();
}

void register_zend_hook_mysqli() {
  mysqliKeysCommands = {"mysqli_affected_rows", "mysqli_autocommit", "mysqli_change_user",
      "mysqli_character_set_name", "mysqli_close", "mysqli_commit", "mysqli_connect_errno",
      "mysqli_connect_error", "mysqli_connect", "mysqli_data_seek", "mysqli_debug",
      "mysqli_dump_debug_info", "mysqli_errno", "mysqli_error_list", "mysqli_error",
      "mysqli_fetch_all", "mysqli_fetch_array", "mysqli_fetch_assoc", "mysqli_fetch_field_direct",
      "mysqli_fetch_field", "mysqli_fetch_fields", "mysqli_fetch_lengths", "mysqli_fetch_object",
      "mysqli_fetch_row", "mysqli_field_count", "mysqli_field_seek", "mysqli_field_tell",
      "mysqli_free_result", "mysqli_get_charset", "mysqli_get_client_info",
      "mysqli_get_client_stats", "mysqli_get_client_version", "mysqli_get_connection_stats",
      "mysqli_get_host_info", "mysqli_get_proto_info", "mysqli_get_server_info",
      "mysqli_get_server_version", "mysqli_info", "mysqli_init", "mysqli_insert_id", "mysql_kill",
      "mysqli_more_results", "mysqli_multi_query", "mysqli_next_result", "mysqli_num_fields",
      "mysqli_num_rows", "mysqli_options", "mysqli_ping", "mysqli_prepare", "mysqli_query",
      "mysqli_real_connect", "mysqli_real_escape_string", "mysqli_real_query",
      "mysqli_reap_async_query", "mysqli_refresh", "mysqli_rollback", "mysqli_select_db",
      "mysqli_set_charset", "mysqli_set_local_infile_default", "mysqli_set_local_infile_handler",
      "mysqli_sqlstate", "mysqli_ssl_set", "mysqli_stat", "mysqli_stmt_init", "mysqli_store_result",
      "mysqli_thread_id", "mysqli_thread_safe", "mysqli_use_result", "mysqli_warning_count"};
  zend_function *old_function;
  for (const auto &item : mysqliKeysCommands) {
    if ((old_function = OPENTELEMETRY_OLD_FN_TABLE(
        CG(function_table), item.c_str())) !=
        nullptr) {
      opentelemetry_mysqli_original_handler_map[item.c_str()] = old_function->internal_function.handler;
      old_function->internal_function.handler = opentelemetry_mysqli_handler;
    }
  }
}

void unregister_zend_hook_mysqli() {
  zend_function *old_function;
  for (const auto &item : mysqliKeysCommands) {
    if ((old_function = OPENTELEMETRY_OLD_FN_TABLE(
        CG(function_table), item.c_str())) !=
        nullptr) {
      old_function->internal_function.handler = opentelemetry_mysqli_original_handler_map[item.c_str()];
    }
  }
}
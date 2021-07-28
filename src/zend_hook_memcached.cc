//
// Created by kilingzhang on 2021/6/24.
//

#include "zend_hook_memcached.h"
#include "php_opentelemetry.h"
#include "zend_types.h"
#include "utils.h"

std::vector<std::string> mecKeysCommands;
std::vector<std::string> mecStrKeysCommands;

void opentelemetry_memcached_handler(INTERNAL_FUNCTION_PARAMETERS) {

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
    set_string_attribute(span->add_attributes(), COMPONENTS_KEY, COMPONENTS_MEMCACHED);
    span->set_parent_span_id(parentId);
    zend_execute_data *caller = execute_data->prev_execute_data;
    if (caller != nullptr && caller->func) {
      trace_caller = find_trace_caller(caller);
      if (!trace_caller.empty()) {
        set_string_attribute(span->add_attributes(), "trace.caller", trace_caller);
      }
    }

    uint32_t arg_count = ZEND_CALL_NUM_ARGS(execute_data);
    std::string command = cmd;
    command = command.append(" ");
    for (int i = 1; i < arg_count + 1; ++i) {
      zval *p = ZEND_CALL_ARG(execute_data, i);
      if (Z_TYPE_P(p) == IS_ARRAY) {
        command = command.append(opentelemetry_json_encode(p));
        continue;
      }

      zval str_p;

      ZVAL_COPY(&str_p, p);
      if (Z_TYPE_P(&str_p) != IS_ARRAY && Z_TYPE_P(&str_p) != IS_STRING) {
        convert_to_string(&str_p);
      }

      std::string str = Z_STRVAL_P(&str_p);

      if (i == 1 && Z_TYPE_P(p) == IS_STRING) {

        set_string_attribute(span->add_attributes(), "memcached.key", str);

        if (std::find(mecStrKeysCommands.begin(), mecStrKeysCommands.end(), cmd) != mecStrKeysCommands.end()) {

          zval *self = &(execute_data->This);
#if PHP_VERSION_ID < 80000
          zval *obj = self;
#else
          zend_object *obj = Z_OBJ_P(self);
#endif
          zval server;
          zval params[1];
          ZVAL_STRING(&params[0], str.c_str());
          zend_call_method(obj, Z_OBJCE_P(self), nullptr, ZEND_STRL("getserverbykey"), &server, 1, params,
                           nullptr);
          zval *str_zval;
          long long port = 0;
          str_zval = zend_hash_str_find(Z_ARRVAL_P(&server), "host", 4);
          std::string host = Z_STRVAL_P(str_zval);
          str_zval = zend_hash_str_find(Z_ARRVAL_P(&server), "port", 4);
          port = Z_LVAL_P(str_zval);

          set_string_attribute(span->add_attributes(), "memcached.peer", host + ":" + std::to_string(port));
          set_string_attribute(span->add_attributes(), "memcached.host", host);
          set_int64_attribute(span->add_attributes(), "memcached.port", port);
          host.clear();
          host.shrink_to_fit();
          zval_dtor(&server);
          zval_dtor(&params[0]);
          zval_dtor(str_zval);
        }
      }

      command = command.append(str);
      command.append(" ");
      str.clear();
      str.shrink_to_fit();
      zval_dtor(&str_p);
    }

    if (!command.empty()) {
      std::transform(command.begin(), command.end(), command.begin(), ::tolower);
      command = trim(command);
      set_string_attribute(span->add_attributes(), "memcached.command", command);
    }

    command.clear();
    command.shrink_to_fit();
  }

  if (!opentelemetry_memcached_original_handler_map.empty() &&
      opentelemetry_memcached_original_handler_map.find(cmd) !=
          opentelemetry_memcached_original_handler_map.end()) {
    opentelemetry_memcached_original_handler_map[cmd](INTERNAL_FUNCTION_PARAM_PASSTHRU);
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

void register_zend_hook_memcached() {
  mecKeysCommands = {"set", "setbykey", "setmulti", "setmultibykey", "add", "addbykey", "replace", "replacebykey",
      "append", "appendbykey", "prepend", "prependbykey", "cas", "casbykey", "get", "getbykey",
      "getmulti", "getmultibykey", "getallkeys", "delete", "deletebykey", "deletemulti",
      "deletemultibykey", "increment", "incrementbykey", "decrement", "decrementbykey", "getstats",
      "ispersistent", "ispristine", "flush", "flushbuffers", "getdelayed", "getdelayedbykey", "fetch",
      "fetchall", "addserver", "addservers", "getoption", "setoption", "setoptions", "getresultcode",
      "getserverlist", "resetserverlist", "getversion", "quit", "setsaslauthdata", "touch",
      "touchbykey"};
  mecStrKeysCommands = {"set", "setbykey", "setmulti", "setmultibykey", "add", "addbykey", "replace", "replacebykey",
      "append", "appendbykey", "prepend", "prependbykey", "cas", "casbykey", "get", "getbykey",
      "getmulti", "getmultibykey", "getallkeys", "delete", "deletebykey", "deletemulti",
      "deletemultibykey", "increment", "incrementbykey", "decrement", "decrementbykey"};
  zend_class_entry *old_class;
  zend_function *old_function;
  if ((old_class = OPENTELEMETRY_OLD_CN("memcached")) != nullptr) {
    for (const auto &item : mecKeysCommands) {
      if ((old_function = OPENTELEMETRY_OLD_FN_TABLE(
          &old_class->function_table, item.c_str())) !=
          nullptr) {
        opentelemetry_memcached_original_handler_map[item.c_str()] = old_function->internal_function.handler;
        old_function->internal_function.handler = opentelemetry_memcached_handler;
      }
    }
  }
}

void unregister_zend_hook_memcached() {
  zend_class_entry *old_class;
  zend_function *old_function;
  if ((old_class = OPENTELEMETRY_OLD_CN("memcached")) != nullptr) {
    for (const auto &item : mecKeysCommands) {
      if ((old_function = OPENTELEMETRY_OLD_FN_TABLE(
          &old_class->function_table, item.c_str())) !=
          nullptr) {
        old_function->internal_function.handler = opentelemetry_memcached_original_handler_map[item.c_str()];
      }
    }
  }
}
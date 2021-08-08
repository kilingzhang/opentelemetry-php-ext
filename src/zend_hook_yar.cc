//
// Created by kilingzhang on 2021/7/10.
//

#include "php_opentelemetry.h"
#include "include/zend_hook_yar.h"
#include "include/utils.h"
#include "zend_types.h"

#include "opentelemetry/proto/trace/v1/trace.pb.h"

using namespace opentelemetry::proto::trace::v1;

std::vector<std::string> clientKeysCommands;
std::vector<std::string> serverKeysCommands;

void opentelemetry_yar_client_handler(INTERNAL_FUNCTION_PARAMETERS) {

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
    span->set_parent_span_id(parentId);
    zend_execute_data *caller = execute_data->prev_execute_data;
    if (caller != nullptr && caller->func) {
      code_stacktrace = find_code_stacktrace(caller);
      if (!code_stacktrace.empty()) {
        set_string_attribute(span->add_attributes(), "code.stacktrace", code_stacktrace);
      }
    }

    set_string_attribute(span->add_attributes(), "rpc.system", COMPONENTS_YAR);

    zval *self = &(execute_data->This);
#if PHP_VERSION_ID < 80000
    zval *obj = self;
#else
    zend_object *obj = Z_OBJ_P(self);
#endif

    zval params[2];
    ZVAL_LONG(&params[0], YAR_OPT_HEADER);
    zval *yar_headers = (zval *) emalloc(sizeof(zval));
    bzero(yar_headers, sizeof(zval));
    array_init(yar_headers);
    std::string traceparent = OPENTELEMETRY_G(provider)->formatTraceParentHeader(span);
    add_next_index_string(yar_headers, ("traceparent: " + traceparent).c_str());
    std::string tracestate = Provider::formatTraceStateHeader();
    add_next_index_string(yar_headers, ("tracestate: " + tracestate).c_str());
    ZVAL_COPY(&params[1], yar_headers);

    zend_call_method(obj, Z_OBJCE_P(self), nullptr, ZEND_STRL("setopt"), nullptr, 2, &params[0], &params[1]);
    zval_dtor(&params[0]);
    zval_dtor(&params[1]);
    zval_dtor(yar_headers);
    efree(yar_headers);

    uint32_t arg_count = ZEND_CALL_NUM_ARGS(execute_data);
    if (arg_count) {

      std::string command;
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
          set_string_attribute(span->add_attributes(), "rpc.method", Z_STRVAL_P(p));
        }

        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        command = command.append(str);
        command.append(" ");
        str.clear();
        str.shrink_to_fit();
        zval_dtor(&str_p);
      }

      if (!command.empty()) {
        std::transform(command.begin(), command.end(), command.begin(), ::tolower);
        command = trim(command);
        set_string_attribute(span->add_attributes(), "rpc.call", command);
        zval *zval_uri = read_object_property(self, "_uri", 0);
        /* 检索uri属性的值 */
        if (zval_uri) {
          zval copy_zval_uri;
          ZVAL_DUP(&copy_zval_uri, zval_uri);
          set_string_attribute(span->add_attributes(), "rpc.service", ZSTR_VAL(Z_STR(copy_zval_uri)));
          zval_dtor(&copy_zval_uri);
        }
      }

      command.clear();
      command.shrink_to_fit();
    }
  }

  if (!opentelemetry_yar_client_original_handler_map.empty() &&
      opentelemetry_yar_client_original_handler_map.find(cmd) !=
          opentelemetry_yar_client_original_handler_map.end()) {
    opentelemetry_yar_client_original_handler_map[cmd](INTERNAL_FUNCTION_PARAM_PASSTHRU);
  }

  if (is_has_provider()) {
    Provider::okEnd(span);
  }

  cmd.clear();
  cmd.shrink_to_fit();
  name.shrink_to_fit();
  class_name.shrink_to_fit();
  function_name.shrink_to_fit();
  code_stacktrace.shrink_to_fit();

}

void opentelemetry_yar_server_handler(INTERNAL_FUNCTION_PARAMETERS) {

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
    span = OPENTELEMETRY_G(provider)->createSpan(name, Span_SpanKind_SPAN_KIND_SERVER);

    span->set_parent_span_id(parentId);
    zend_execute_data *caller = execute_data->prev_execute_data;
    if (caller != nullptr && caller->func) {
      code_stacktrace = find_code_stacktrace(caller);
      if (!code_stacktrace.empty()) {
        set_string_attribute(span->add_attributes(), "code.stacktrace", code_stacktrace);
      }
    }
  }

  set_string_attribute(span->add_attributes(), "rpc.system", COMPONENTS_YAR);

  if (!opentelemetry_yar_server_original_handler_map.empty() &&
      opentelemetry_yar_server_original_handler_map.find(cmd) !=
          opentelemetry_yar_server_original_handler_map.end()) {
    opentelemetry_yar_server_original_handler_map[cmd](INTERNAL_FUNCTION_PARAM_PASSTHRU);
  }

  if (is_has_provider()) {
    Provider::okEnd(span);
  }

  cmd.clear();
  cmd.shrink_to_fit();
  name.shrink_to_fit();
  class_name.shrink_to_fit();
  function_name.shrink_to_fit();
  code_stacktrace.shrink_to_fit();

}

void register_zend_hook_yar() {
  clientKeysCommands = {"__call"};
  serverKeysCommands = {"__construct"};
  zend_class_entry *old_class;
  zend_function *old_function;

  if ((old_class = OPENTELEMETRY_OLD_CN("yar_client")) != nullptr) {
    for (const auto &item : clientKeysCommands) {
      if ((old_function = OPENTELEMETRY_OLD_FN_TABLE(
          &old_class->function_table, item.c_str())) !=
          nullptr) {
        opentelemetry_yar_client_original_handler_map[item.c_str()] = old_function->internal_function.handler;
        old_function->internal_function.handler = opentelemetry_yar_client_handler;
      }
    }
  }

  if ((old_class = OPENTELEMETRY_OLD_CN("yar_server")) != nullptr) {
    for (const auto &item : serverKeysCommands) {
      if ((old_function = OPENTELEMETRY_OLD_FN_TABLE(
          &old_class->function_table, item.c_str())) !=
          nullptr) {
        opentelemetry_yar_server_original_handler_map[item.c_str()] = old_function->internal_function.handler;
        old_function->internal_function.handler = opentelemetry_yar_server_handler;
      }
    }
  }
}

void unregister_zend_hook_yar() {
  zend_class_entry *old_class;
  zend_function *old_function;
  if ((old_class = OPENTELEMETRY_OLD_CN("yar_client")) != nullptr) {
    for (const auto &item : clientKeysCommands) {
      if ((old_function = OPENTELEMETRY_OLD_FN_TABLE(
          &old_class->function_table, item.c_str())) !=
          nullptr) {
        old_function->internal_function.handler = opentelemetry_yar_client_original_handler_map[item.c_str()];
      }
    }
  }

  if ((old_class = OPENTELEMETRY_OLD_CN("yar_server")) != nullptr) {
    for (const auto &item : serverKeysCommands) {
      if ((old_function = OPENTELEMETRY_OLD_FN_TABLE(
          &old_class->function_table, item.c_str())) !=
          nullptr) {
        old_function->internal_function.handler = opentelemetry_yar_server_original_handler_map[item.c_str()];
      }
    }
  }
  clientKeysCommands.clear();
  serverKeysCommands.clear();
}
//
// Created by kilingzhang on 2021/6/24.
//

#include "php_opentelemetry.h"
#include "include/zend_hook_error.h"
#include "include/utils.h"
#include "zend_types.h"
#include "zend_errors.h"

#include "opentelemetry/proto/trace/v1/trace.pb.h"

using namespace opentelemetry::proto::trace::v1;

#if PHP_VERSION_ID < 80000

extern void (*opentelemetry_original_zend_error_cb)(int type, const char *error_filename, const uint error_lineno,
                                                    const char *format, va_list args);

void (*opentelemetry_original_zend_error_cb)(int type, const char *error_filename, const uint error_lineno,
                                             const char *format, va_list args) = nullptr;

void opentelemetry_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format,
                            va_list args) {
#else

  extern void (*opentelemetry_original_zend_error_cb)(int type, const char *error_filename, const uint32_t error_lineno,
                                                      zend_string *message);

  void (*opentelemetry_original_zend_error_cb)(int type, const char *error_filename, const uint32_t error_lineno,
                                               zend_string *message) = nullptr;

  void opentelemetry_error_cb(int type, const char *error_filename, const uint32_t error_lineno, zend_string *message) {
#endif

  bool isCollector = true;
  std::string level;
  switch (type) {
    case E_ERROR:
    case E_PARSE:
    case E_CORE_ERROR:
    case E_COMPILE_ERROR:
    case E_USER_ERROR:
    case E_RECOVERABLE_ERROR:level = "PHP_ERROR_FATAL";
      break;
    case E_WARNING:
    case E_CORE_WARNING:
    case E_COMPILE_WARNING:
    case E_USER_WARNING:level = "PHP_ERROR_WARNING";
      break;
    case E_NOTICE:
    case E_USER_NOTICE:
    case E_STRICT:
    case E_DEPRECATED:
    case E_USER_DEPRECATED:level = "PHP_ERROR_NOTICE";
      break;
    default:level = "PHP_ERROR_" + std::to_string(type);
      break;
  }

  if (level == "PHP_ERROR_WARNING" && is_equal("E_ERROR", OPENTELEMETRY_G(error_level))) {
    isCollector = false;
  }

  if (level == "PHP_ERROR_" && (
      is_equal("E_ERROR", OPENTELEMETRY_G(error_level)) ||
          is_equal("E_WARNING", OPENTELEMETRY_G(error_level))
  )) {
    isCollector = false;
  }

  if (isCollector && is_has_provider()) {

    Span *span;
    std::string function_name;
    zend_execute_data *caller = EG(current_execute_data);

    if (caller == nullptr || caller->func == nullptr) {
      function_name = OPENTELEMETRY_G(provider)->firstOneSpan()->name();
    } else {
      if (caller->func->common.function_name) {
        function_name = find_trace_add_scope_name(caller->func->common.function_name,
                                                  caller->func->common.scope,
                                                  caller->func->common.fn_flags);
      } else if (caller->func->internal_function.function_name) {
        function_name = find_trace_add_scope_name(caller->func->internal_function.function_name,
                                                  caller->func->internal_function.scope,
                                                  caller->func->internal_function.fn_flags);
      }
    }

    std::string parentId = OPENTELEMETRY_G(provider)->latestSpan().span_id();
    span = OPENTELEMETRY_G(provider)->createSpan(function_name, Span_SpanKind_SPAN_KIND_INTERNAL);
    span->set_parent_span_id(parentId);

    bool isError = EG(error_reporting) & type;
    std::string stacktrace = error_filename;
    stacktrace.append(":").append(std::to_string(error_lineno));
    std::string error_message;

#if PHP_VERSION_ID < 80000
    char *msg;
    va_list args_copy;
    va_copy(args_copy, args);
    vspprintf(&msg, 0, format, args_copy);
    va_end(args_copy);
    error_message = msg;
    efree(msg);
#else
    error_message = message->val;
#endif

    std::string code_stacktrace = find_code_stacktrace(caller);
    set_string_attribute(span->add_attributes(), "exception.type", level);
    set_string_attribute(span->add_attributes(), "exception.message", error_message);
    set_string_attribute(span->add_attributes(), "exception.stacktrace", stacktrace);
    set_string_attribute(span->add_attributes(), "code.stacktrace", code_stacktrace);

    if (isError) {
      if (code_stacktrace.empty()) {
        Provider::errorEnd(OPENTELEMETRY_G(provider)->firstOneSpan(), error_message);
      }
      Provider::errorEnd(span, error_message);
    } else {
      Provider::okEnd(span);
    }

    code_stacktrace.shrink_to_fit();
    function_name.shrink_to_fit();
    stacktrace.shrink_to_fit();
    error_message.shrink_to_fit();

  }
  level.shrink_to_fit();

#if PHP_VERSION_ID < 80000
  opentelemetry_original_zend_error_cb(type, error_filename, error_lineno, format, args
  );
#else
  opentelemetry_original_zend_error_cb(type, error_filename, error_lineno, message);
#endif
}

void register_zend_hook_error() {
  //error时的通知
  opentelemetry_original_zend_error_cb = zend_error_cb;
  zend_error_cb = opentelemetry_error_cb;
}

void unregister_zend_hook_error() {
  zend_error_cb = opentelemetry_original_zend_error_cb;
}

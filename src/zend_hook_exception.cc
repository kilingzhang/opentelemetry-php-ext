//
// Created by kilingzhang on 2021/6/24.
//

#include "php_opentelemetry.h"
#include "include/zend_hook_exception.h"
#include "include/utils.h"
#include "zend_exceptions.h"
#include "zend_compile.h"
#include "zend_types.h"

#ifdef HAVE_DTRACE
#include <zend_dtrace_gen.h>
#endif /* HAVE_DTRACE */

#include "opentelemetry/proto/trace/v1/trace.pb.h"

using namespace opentelemetry::proto::trace::v1;

#if PHP_VERSION_ID < 80000

extern void (*opentelemetry_original_zend_throw_exception_hook)(zval *ex);

void (*opentelemetry_original_zend_throw_exception_hook)(zval *ex) = nullptr;

void opentelemetry_throw_exception_hook(zval *exception) {
#ifdef HAVE_DTRACE
    if (DTRACE_EXCEPTION_THROWN_ENABLED()) {
        if (exception != nullptr) {
            DTRACE_EXCEPTION_THROWN(ZSTR_VAL(Z_OBJ_P(exception)->ce->name));
        } else {
            DTRACE_EXCEPTION_THROWN(nullptr);
        }
    }
#endif /* HAVE_DTRACE */

    if (is_has_provider()) {
        std::string function_name;
        std::string message;
        zend_execute_data *caller = EG(current_execute_data);
        if (exception != nullptr) {
            zval *zval_exception_message = read_object_property(exception, "message", 0);
            zval *zval_exception_file = read_object_property(exception, "file", 0);
            zval *zval_exception_code = read_object_property(exception, "code", 0);
            zval *zval_exception_line = read_object_property(exception, "line", 0);

            zval copy_zval_exception_message;
            zval copy_zval_exception_file;
            zval copy_zval_exception_code;
            zval copy_zval_exception_line;
            ZVAL_DUP(&copy_zval_exception_message, zval_exception_message);
            ZVAL_DUP(&copy_zval_exception_file, zval_exception_file);
            ZVAL_DUP(&copy_zval_exception_code, zval_exception_code);
            ZVAL_DUP(&copy_zval_exception_line, zval_exception_line);
            std::string exception_message = ZSTR_VAL(Z_STR(copy_zval_exception_message));
            std::string exception_file = ZSTR_VAL(Z_STR(copy_zval_exception_file));
            long exception_code = Z_LVAL(copy_zval_exception_code);
            long exception_line = Z_LVAL(copy_zval_exception_line);

            if (caller == nullptr || caller->func == nullptr) {
                function_name = OPENTELEMETRY_G(provider)->firstOneSpan()->name();
            } else {
                if (caller->func->common.function_name) {
                    function_name = find_trace_add_scope_name(
                        caller->func->common.function_name, caller->func->common.scope, caller->func->common.fn_flags);
                } else if (caller->func->internal_function.function_name) {
                    function_name = find_trace_add_scope_name(caller->func->internal_function.function_name,
                                                              caller->func->internal_function.scope,
                                                              caller->func->internal_function.fn_flags);
                }
            }

            std::string parentId = OPENTELEMETRY_G(provider)->latestSpan().span_id();
            auto *span = OPENTELEMETRY_G(provider)->createSpan(function_name, Span_SpanKind_SPAN_KIND_INTERNAL);
            span->set_parent_span_id(parentId);
            Provider::errorEnd(span, exception_message);

            set_string_attribute(span->add_attributes(), "exception.type", Z_OBJ_P(exception)->ce->name->val);
            set_string_attribute(span->add_attributes(), "exception.message", exception_message);
            set_string_attribute(span->add_attributes(),
                                 "exception.stacktrace",
                                 exception_file.append(":").append(std::to_string(exception_line)));

            // 记录redis异常信息
            if (is_equal("RedisException", Z_OBJ_P(exception)->ce->name->val)) {
                OPENTELEMETRY_G(provider)->setRedisException(exception_message);
            }

            std::string code_stacktrace = find_code_stacktrace(caller);
            set_string_attribute(span->add_attributes(), "code.stacktrace", code_stacktrace);
            code_stacktrace.shrink_to_fit();
            function_name.shrink_to_fit();
            exception_message.shrink_to_fit();
            exception_file.shrink_to_fit();
            zval_dtor(&copy_zval_exception_message);
            zval_dtor(&copy_zval_exception_file);
            zval_dtor(&copy_zval_exception_code);
            zval_dtor(&copy_zval_exception_line);
        }
    }

    if (opentelemetry_original_zend_throw_exception_hook != nullptr) {
        opentelemetry_original_zend_throw_exception_hook(exception);
    }
}

#else

extern void (*opentelemetry_original_zend_throw_exception_hook)(zend_object *ex);

void (*opentelemetry_original_zend_throw_exception_hook)(zend_object *ex) = nullptr;

void opentelemetry_throw_exception_hook(zend_object *ex) {
    if (opentelemetry_original_zend_throw_exception_hook != nullptr) {
        opentelemetry_original_zend_throw_exception_hook(ex);
    }
}

#endif

void register_zend_hook_exception() {
    // 引发异常时的通知
    opentelemetry_original_zend_throw_exception_hook = zend_throw_exception_hook;
    zend_throw_exception_hook = opentelemetry_throw_exception_hook;
}

void unregister_zend_hook_exception() {
    zend_throw_exception_hook = opentelemetry_original_zend_throw_exception_hook;
}

//
// Created by kilingzhang on 2021/6/16.
//

#ifndef OPENTELEMETRY_CORE_H
#define OPENTELEMETRY_CORE_H

#include "php_opentelemetry.h"
#include "zend_execute.h"
#include "include/otel_exporter.h"

/**
 *
 * @param request
 */
void exporterOpenTelemetry();

void init_consumers();

void clean_consumers();

[[noreturn]] void consumer(int i, const std::string &name, OtelExporter *exporter);

/**
 *
 */
void opentelemetry_module_init();

/**
 *
 */
void opentelemetry_module_shutdown();

/**
 *
 */
void opentelemetry_request_init();

/**
 *
 */
void opentelemetry_request_shutdown();

/**
 *
 */
void start_tracer(std::string traceparent,
                  std::string tracebaggage,
                  opentelemetry::proto::trace::v1::Span_SpanKind kind);

/**
 *
 */
void shutdown_tracer();

void tiros_trace_execute(zend_bool is_internal, INTERNAL_FUNCTION_PARAMETERS);

static void (*tiros_original_zend_execute_ex)(zend_execute_data *execute_data);

void tiros_trace_execute_ex(zend_execute_data *execute_data TSRMLS_DC);

static void (*tiros_original_zend_execute_internal)(zend_execute_data *execute_data, zval *return_value);

void tiros_trace_execute_internal(INTERNAL_FUNCTION_PARAMETERS);

#endif  // OPENTELEMETRY_CORE_H

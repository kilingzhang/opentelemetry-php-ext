//
// Created by kilingzhang on 2021/6/16.
//

#ifndef OPENTELEMETRY_CORE_H
#define OPENTELEMETRY_CORE_H

#include "php_opentelemetry.h"

/**
 *
 */
[[noreturn]] void consumer(const std::string &name);

/**
 *
 * @param tracer
 */
void exporterOpentelemetry();

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
void start_tracer(std::string traceparent, std::string tracestate, opentelemetry::proto::trace::v1::Span_SpanKind kind);

/**
 *
 */
void shutdown_tracer();

#endif //OPENTELEMETRY_CORE_H

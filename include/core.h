//
// Created by kilingzhang on 2021/6/16.
//

#ifndef OPENTELEMETRY_CORE_H
#define OPENTELEMETRY_CORE_H

#include "php_opentelemetry.h"
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

#endif  // OPENTELEMETRY_CORE_H

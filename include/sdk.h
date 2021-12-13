//
// Created by kilingzhang on 2021/6/16.
//

#ifndef OPENTELEMETRY_SDK_H
#define OPENTELEMETRY_SDK_H

#include "php_opentelemetry.h"

PHP_FUNCTION (opentelemetry_start_cli_tracer);

PHP_FUNCTION (opentelemetry_shutdown_cli_tracer);

PHP_FUNCTION (opentelemetry_get_traceparent);

PHP_FUNCTION (opentelemetry_get_tracestate);

PHP_FUNCTION (opentelemetry_add_tracestate);

PHP_FUNCTION (opentelemetry_set_sample_ratio_based);

PHP_FUNCTION (opentelemetry_get_service_name);

PHP_FUNCTION (opentelemetry_get_service_ip);

PHP_FUNCTION (opentelemetry_get_ppid);

PHP_FUNCTION (opentelemetry_get_unix_nano);

PHP_FUNCTION (opentelemetry_add_span);

PHP_FUNCTION (opentelemetry_add_resource_attribute);

PHP_FUNCTION (opentelemetry_add_attribute);

PHP_FUNCTION (opentelemetry_add_event);

#endif //OPENTELEMETRY_SDK_H

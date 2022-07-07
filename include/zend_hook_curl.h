//
// Created by kilingzhang on 2021/6/24.
//

#ifndef OPENTELEMETRY_ZEND_HOOK_CURL_H
#define OPENTELEMETRY_ZEND_HOOK_CURL_H

#include "php_opentelemetry.h"

#define OPENTELEMETRY_CURLOPT_HTTPHEADER 9923

static void (*opentelemetry_original_curl_exec)(INTERNAL_FUNCTION_PARAMETERS);

void opentelemetry_curl_exec_handler(INTERNAL_FUNCTION_PARAMETERS);

static void (*opentelemetry_original_curl_setopt)(INTERNAL_FUNCTION_PARAMETERS);

void opentelemetry_curl_setopt_handler(INTERNAL_FUNCTION_PARAMETERS);

static void (*opentelemetry_original_curl_setopt_array)(INTERNAL_FUNCTION_PARAMETERS);

void opentelemetry_curl_setopt_array_handler(INTERNAL_FUNCTION_PARAMETERS);

static void (*opentelemetry_original_curl_close)(INTERNAL_FUNCTION_PARAMETERS);

void opentelemetry_curl_close_handler(INTERNAL_FUNCTION_PARAMETERS);

void register_zend_hook_curl();

void unregister_zend_hook_curl();

#endif  // OPENTELEMETRY_ZEND_HOOK_CURL_H

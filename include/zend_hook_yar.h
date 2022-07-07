//
// Created by kilingzhang on 2021/7/10.
//

#ifndef OPENTELEMETRY_ZEND_HOOK_YAR_H
#define OPENTELEMETRY_ZEND_HOOK_YAR_H

#include "php_opentelemetry.h"
#include "zend_types.h"

#include "tsl/robin_map.h"

#define YAR_OPT_HEADER (1 << 4)
static tsl::robin_map<std::string, void (*)(INTERNAL_FUNCTION_PARAMETERS)>
    opentelemetry_yar_client_original_handler_map;
static tsl::robin_map<std::string, void (*)(INTERNAL_FUNCTION_PARAMETERS)>
    opentelemetry_yar_server_original_handler_map;

void opentelemetry_yar_client_handler(INTERNAL_FUNCTION_PARAMETERS);

void opentelemetry_yar_server_handler(INTERNAL_FUNCTION_PARAMETERS);

void register_zend_hook_yar();

void unregister_zend_hook_yar();

#endif  // OPENTELEMETRY_ZEND_HOOK_YAR_H

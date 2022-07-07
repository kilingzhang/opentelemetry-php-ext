//
// Created by kilingzhang on 2021/7/10.
//

#ifndef OPENTELEMETRY_ZEND_HOOK_MYSQLI_H
#define OPENTELEMETRY_ZEND_HOOK_MYSQLI_H

#include "php_opentelemetry.h"
#include "zend_types.h"

#include "tsl/robin_map.h"

static tsl::robin_map<std::string, void (*)(INTERNAL_FUNCTION_PARAMETERS)> opentelemetry_mysqli_original_handler_map;

void opentelemetry_mysqli_handler(INTERNAL_FUNCTION_PARAMETERS);

void register_zend_hook_mysqli();

void unregister_zend_hook_mysqli();

#endif  // OPENTELEMETRY_ZEND_HOOK_MYSQLI_H

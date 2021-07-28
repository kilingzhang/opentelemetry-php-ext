//
// Created by kilingzhang on 2021/7/10.
//

#ifndef OPENTELEMETRY_ZEND_HOOK_PDO_H
#define OPENTELEMETRY_ZEND_HOOK_PDO_H

#include "php_opentelemetry.h"
#include "zend_types.h"

static tsl::robin_map<std::string, void (*)(INTERNAL_FUNCTION_PARAMETERS)> opentelemetry_pdo_original_handler_map;

void opentelemetry_pdo_handler(INTERNAL_FUNCTION_PARAMETERS);

void register_zend_hook_pdo();

void unregister_zend_hook_pdo();

#endif //OPENTELEMETRY_ZEND_HOOK_PDO_H

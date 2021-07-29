//
// Created by kilingzhang on 2021/6/24.
//

#ifndef OPENTELEMETRY_ZEND_HOOK_MEMCACHED_H
#define OPENTELEMETRY_ZEND_HOOK_MEMCACHED_H

#include "php_opentelemetry.h"
#include "zend_types.h"

#include "tsl/robin_map.h"

static tsl::robin_map<std::string, void (*)(INTERNAL_FUNCTION_PARAMETERS)> opentelemetry_memcached_original_handler_map;

void opentelemetry_memcached_handler(INTERNAL_FUNCTION_PARAMETERS);

void register_zend_hook_memcached();

void unregister_zend_hook_memcached();

#endif //OPENTELEMETRY_ZEND_HOOK_MEMCACHED_H

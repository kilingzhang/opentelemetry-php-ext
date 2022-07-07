//
// Created by kilingzhang on 2021/6/24.
//

#ifndef OPENTELEMETRY_ZEND_HOOK_ERROR_H
#define OPENTELEMETRY_ZEND_HOOK_ERROR_H

#include "php_opentelemetry.h"

void register_zend_hook_error();

void unregister_zend_hook_error();

#endif  // OPENTELEMETRY_ZEND_HOOK_ERROR_H

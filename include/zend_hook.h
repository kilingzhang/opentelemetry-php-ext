//
// Created by kilingzhang on 2021/6/23.
//

#ifndef OPENTELEMETRY_ZEND_HOOK_H
#define OPENTELEMETRY_ZEND_HOOK_H

#include "php_opentelemetry.h"

void register_zend_hook();

void unregister_zend_hook();

#endif  // OPENTELEMETRY_ZEND_HOOK_H

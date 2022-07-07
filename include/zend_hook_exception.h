//
// Created by kilingzhang on 2021/6/24.
//

#ifndef OPENTELEMETRY_ZEND_HOOK_EXCEPTION_H
#define OPENTELEMETRY_ZEND_HOOK_EXCEPTION_H

#include "php_opentelemetry.h"

void register_zend_hook_exception();

void unregister_zend_hook_exception();

#if PHP_VERSION_ID < 80000

void opentelemetry_throw_exception_hook(zval *exception);

#else

void opentelemetry_throw_exception_hook(zend_object *ex);

#endif

#endif  // OPENTELEMETRY_ZEND_HOOK_EXCEPTION_H

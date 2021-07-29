//
// Created by kilingzhang on 2021/6/23.
//

#include "include/zend_hook.h"
#include "include/zend_hook_error.h"
#include "include/zend_hook_exception.h"
#include "include/zend_hook_curl.h"
#include "include/zend_hook_yar.h"
#include "include/zend_hook_memcached.h"
#include "include/zend_hook_redis.h"
#include "include/zend_hook_pdo.h"
#include "include/zend_hook_pdo_statement.h"
#include "include/zend_hook_mysqli.h"

void register_zend_hook() {

  if (OPENTELEMETRY_G(enable_error)) {
    register_zend_hook_error();
  }
  if (OPENTELEMETRY_G(enable_exception)) {
    register_zend_hook_exception();
  }
  if (OPENTELEMETRY_G(enable_curl)) {
    register_zend_hook_curl();
  }
  if (OPENTELEMETRY_G(enable_yar)) {
    register_zend_hook_yar();
  }
  if (OPENTELEMETRY_G(enable_memcached)) {
    register_zend_hook_memcached();
  }
  if (OPENTELEMETRY_G(enable_redis)) {
    register_zend_hook_redis();
  }
  if (OPENTELEMETRY_G(enable_mysql)) {
    register_zend_hook_pdo();
    register_zend_hook_pdo_statement();
    register_zend_hook_mysqli();
  }

}

void unregister_zend_hook() {

  if (OPENTELEMETRY_G(enable_error)) {
    unregister_zend_hook_error();
  }
  if (OPENTELEMETRY_G(enable_exception)) {
    unregister_zend_hook_exception();
  }
  if (OPENTELEMETRY_G(enable_curl)) {
    unregister_zend_hook_curl();
  }
  if (OPENTELEMETRY_G(enable_yar)) {
    unregister_zend_hook_yar();
  }
  if (OPENTELEMETRY_G(enable_memcached)) {
    unregister_zend_hook_memcached();
  }
  if (OPENTELEMETRY_G(enable_redis)) {
    unregister_zend_hook_redis();
  }
  if (OPENTELEMETRY_G(enable_mysql)) {
    unregister_zend_hook_pdo();
    unregister_zend_hook_pdo_statement();
    unregister_zend_hook_mysqli();
  }

}
/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2017 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

#include "php_opentelemetry.h"
#include "include/core.h"
#include "include/sdk.h"
extern "C" { void *__dso_handle = 0; }

ZEND_DECLARE_MODULE_GLOBALS(opentelemetry)
PHP_INI_BEGIN()
        STD_PHP_INI_ENTRY("opentelemetry.service_name", PHP_OPENTELEMETRY_SERVICE_NAME, PHP_INI_ALL, OnUpdateString, service_name, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.service_name_key", PHP_OPENTELEMETRY_SERVICE_NAME_KEY, PHP_INI_ALL, OnUpdateString, service_name_key, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.enable", "0", PHP_INI_PERDIR, OnUpdateBool, enable, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.enable_exception", "0", PHP_INI_PERDIR, OnUpdateBool, enable_exception, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.enable_error", "0", PHP_INI_PERDIR, OnUpdateBool, enable_error, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.enable_curl", "0", PHP_INI_PERDIR, OnUpdateBool, enable_curl, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.enable_memcached", "0", PHP_INI_PERDIR, OnUpdateBool, enable_memcached, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.enable_redis", "0", PHP_INI_PERDIR, OnUpdateBool, enable_redis, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.enable_mysql", "0", PHP_INI_PERDIR, OnUpdateBool, enable_mysql, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.enable_yar", "0", PHP_INI_PERDIR, OnUpdateBool, enable_yar, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.enable_collect", "0", PHP_INI_PERDIR, OnUpdateBool, enable_collect, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.debug", "0", PHP_INI_PERDIR, OnUpdateBool, debug, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.cli_enable", "0", PHP_INI_PERDIR, OnUpdateBool, cli_enable, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.sample_ratio_based", "1", PHP_INI_PERDIR, OnUpdateLong, sample_ratio_based, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.max_time_consuming", PHP_OPENTELEMETRY_MAX_TIME_CONSUMING, PHP_INI_PERDIR, OnUpdateLong, max_time_consuming, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.error_level", PHP_OPENTELEMETRY_ERROR_LEVEL, PHP_INI_PERDIR, OnUpdateString, error_level, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.log_path", PHP_OPENTELEMETRY_LOG_PATH, PHP_INI_PERDIR, OnUpdateString, log_path, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.unix_socket", PHP_OPENTELEMETRY_UNIX_SOCKET, PHP_INI_PERDIR, OnUpdateString, unix_socket, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.grpc_endpoint", PHP_OPENTELEMETRY_GRPC_ENDPOINT, PHP_INI_PERDIR, OnUpdateString, grpc_endpoint, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.grpc_max_message_size", "102400", PHP_INI_PERDIR, OnUpdateLong, grpc_max_message_size, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.grpc_max_queue_length", "1024", PHP_INI_PERDIR, OnUpdateLong, grpc_max_queue_length, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.grpc_consumer", "20", PHP_INI_PERDIR, OnUpdateLong, grpc_consumer, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.grpc_timeout_milliseconds", "100", PHP_INI_PERDIR, OnUpdateLong, grpc_timeout_milliseconds, zend_opentelemetry_globals, opentelemetry_globals)
        STD_PHP_INI_ENTRY("opentelemetry.environment", PHP_OPENTELEMETRY_ENVIRONMENT, PHP_INI_PERDIR, OnUpdateString, environment, zend_opentelemetry_globals, opentelemetry_globals)
PHP_INI_END()


//static void php_opentelemetry_init_globals(zend_opentelemetry_globals *opentelemetry_globals) {
//  opentelemetry_globals->grpc_timeout_milliseconds = 100;
//  opentelemetry_globals->grpc_max_message_size = 102400;
//}

PHP_MINIT_FUNCTION (opentelemetry) {
  REGISTER_INI_ENTRIES();
  opentelemetry_module_init();
  return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION (opentelemetry) {
  opentelemetry_module_shutdown();
  UNREGISTER_INI_ENTRIES();
  return SUCCESS;
}

PHP_RINIT_FUNCTION (opentelemetry) {
#if defined(COMPILE_DL_OPENTELEMETRY) && defined(ZTS)
  ZEND_TSRMLS_CACHE_UPDATE();
#endif
  opentelemetry_request_init();
  return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION (opentelemetry) {
  opentelemetry_request_shutdown();
  return SUCCESS;
}

PHP_MINFO_FUNCTION (opentelemetry) {
  php_info_print_table_start();
  php_info_print_table_header(2, "opentelemetry support", "enabled");
  php_info_print_table_header(2, "opentelemetry version", PHP_OPENTELEMETRY_VERSION);
  php_info_print_table_end();
}

const zend_function_entry opentelemetry_functions[] = {
    PHP_FE(opentelemetry_start_cli_tracer, NULL)
    PHP_FE(opentelemetry_shutdown_cli_tracer, NULL)
    PHP_FE(opentelemetry_get_traceparent, NULL)
    PHP_FE(opentelemetry_add_tracestate, NULL)
    PHP_FE(opentelemetry_get_tracestate, NULL)
    PHP_FE(opentelemetry_set_sample_ratio_based, NULL)
    PHP_FE(opentelemetry_get_service_name, NULL)
    PHP_FE(opentelemetry_get_service_ip, NULL)
    PHP_FE(opentelemetry_get_ppid, NULL)
    PHP_FE_END    /* Must be the last line in opentelemetry_functions[] */
};

zend_module_entry opentelemetry_module_entry = {
    STANDARD_MODULE_HEADER,
    "opentelemetry",
    opentelemetry_functions,
    PHP_MINIT(opentelemetry),
    PHP_MSHUTDOWN(opentelemetry),
    PHP_RINIT(opentelemetry),        /* Replace with NULL if there's nothing to do at request start */
    PHP_RSHUTDOWN(opentelemetry),    /* Replace with NULL if there's nothing to do at request end */
    PHP_MINFO(opentelemetry),
    PHP_OPENTELEMETRY_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_OPENTELEMETRY
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(opentelemetry)
#endif


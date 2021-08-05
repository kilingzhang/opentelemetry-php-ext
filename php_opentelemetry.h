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

/* $Id$ */

#ifndef PHP_OPENTELEMETRY_H
#define PHP_OPENTELEMETRY_H

#ifdef __cplusplus
#define OPENTELEMETRY_BEGIN_EXTERN_C() extern "C" {
#define OPENTELEMETRY_END_EXTERN_C() }
#else
#define OPENTELEMETRY_BEGIN_EXTERN_C()
#define OPENTELEMETRY_END_EXTERN_C()
#endif

#include "include/provider.h"
#include  <string>

OPENTELEMETRY_BEGIN_EXTERN_C()

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

//php zend
#include "php.h"
#include "php_ini.h"
#include "main/SAPI.h"
#include "zend.h"
#include "zend_API.h"
#include "zend_errors.h"
#include "zend_types.h"
#include <zend_exceptions.h>
#include "zend_compile.h"
#include "zend_interfaces.h"
#include "zend_types.h"
#include "zend_smart_str.h"
#include "Zend/zend_smart_str.h"
#include "Zend/zend_builtin_functions.h"
#include "zend_exceptions.h"

#include <curl/curl.h>
#include "ext/standard/url.h"
#if PHP_VERSION_ID >= 80000
#include "ext/curl/php_curl.h"
#endif
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_array.h"
#include "ext/standard/php_var.h"
#include "ext/pdo/php_pdo_driver.h"
#include "ext/json/php_json.h"
#include "ext/standard/php_smart_string.h"

#ifdef MYSQLI_USE_MYSQLND
#include "ext/mysqli/php_mysqli_structs.h"
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

extern zend_module_entry opentelemetry_module_entry;
#define phpext_opentelemetry_ptr &opentelemetry_module_entry

#define PHP_OPENTELEMETRY_VERSION "0.0.1" /* Replace with version number for your extension */
#define DEFAULT_ETH_INF "eth0"
#define PHP_OPENTELEMETRY_SERVICE_NAME "opentelemetry"
#define PHP_OPENTELEMETRY_SERVICE_NAME_KEY "SERVICE_NAME"
#define PHP_OPENTELEMETRY_LOG_PATH "/var/log/opentelemetry"
#define PHP_OPENTELEMETRY_UNIX_SOCKET "/var/run/opentelemetry-agent.sock"
#define PHP_OPENTELEMETRY_GRPC_ENDPOINT "127.0.0.1:4317"
#define PHP_OPENTELEMETRY_ENVIRONMENT "staging"
#define PHP_OPENTELEMETRY_ERROR_LEVEL "E_ERROR"
#define PHP_OPENTELEMETRY_MAX_TIME_CONSUMING "1000"
#define PHP_IS_DEBUG OPENTELEMETRY_DEBUG

#define OPENTELEMETRY_START_CLI_TRACER_FUNCTION_NAME  "opentelemetry_start_cli_tracer"
#define OPENTELEMETRY_SHUTDOWN_CLI_TRACER_FUNCTION_NAME  "opentelemetry_shutdown_cli_tracer"
#define BOOST_UUID_RANDOM_PROVIDER_FORCE_POSIX

#define OPENTELEMETRY_OLD_FN(n) static_cast<zend_function *>(zend_hash_str_find_ptr(CG(function_table), n, sizeof(n) - 1))
#define OPENTELEMETRY_OLD_FN_TABLE(table, n) static_cast<zend_function *>(zend_hash_str_find_ptr(table, n, strlen(n)))
#define OPENTELEMETRY_OLD_CN(n) static_cast<zend_class_entry *>(zend_hash_str_find_ptr(CG(class_table), n, sizeof(n) - 1))

#define REDIS_KEYS "connect|dump|exists|expire|expireat|move|persist|pexpire|pexpireat|pttl|rename|renamenx|sort|ttl|type|append|bitcount|bitfield|decr|decrby|get|getbit|getrange|getset|incr|incrby|incrbyfloat|psetex|set|setbit|setex|setnx|setrange|strlen|bitop|hdel|hexists|hget|hgetall|hincrby|hincrbyfloat|hkeys|hlen|hmget|hmset|hscan|hset|hsetnx|hvals|hstrlen|lindex|linsert|llen|lpop|lpush|lpushx|lrange|lrem|lset|ltrim|rpop|rpush|rpushx|sadd|scard|sismember|smembers|spop|srandmember|srem|sscan|zadd|zcard|zcount|zincrby|zrange|zrangebyscore|zrank|zrem|zremrangebyrank|zremrangebyscore|zrevrange|zrevrangebyscore|zrevrank|zscore|zscan|zrangebylex|zrevrangebylex|zremrangebylex|zlexcount|pfadd|watch|geoadd|geohash|geopos|geodist|georadius|georadiusbymember"
#define PDOSTATEMENT_KEYS "bindcolumn|bindparam|bindvalue|closecursor|columncount|debugdumpparams|errorcode|errorinfo|execute|fetch|fetchall|fetchcolumn|fetchobject|getattribute|getcolumnMeta|nextrowset|rowcount|setattribute|setfetchmode"
#define PDO_KEYS "__construct|begintransaction|commit|errorcode|errorinfo|exec|getattribute|getavailabledrivers|intransaction|lastinsertid|prepare|query|quote|rollback|setattribute"
#define MYSQLI_KEYS "mysqli_affected_rows|mysqli_autocommit|mysqli_change_user|mysqli_character_set_name|mysqli_close|mysqli_commit|mysqli_connect_errno|mysqli_connect_error|mysqli_connect|mysqli_data_seek|mysqli_debug|mysqli_dump_debug_info|mysqli_errno|mysqli_error_list|mysqli_error|mysqli_fetch_all|mysqli_fetch_array|mysqli_fetch_assoc|mysqli_fetch_field_direct|mysqli_fetch_field|mysqli_fetch_fields|mysqli_fetch_lengths|mysqli_fetch_object|mysqli_fetch_row|mysqli_field_count|mysqli_field_seek|mysqli_field_tell|mysqli_free_result|mysqli_get_charset|mysqli_get_client_info|mysqli_get_client_stats|mysqli_get_client_version|mysqli_get_connection_stats|mysqli_get_host_info|mysqli_get_proto_info|mysqli_get_server_info|mysqli_get_server_version|mysqli_info|mysqli_init|mysqli_insert_id|mysql_kill|mysqli_more_results|mysqli_multi_query|mysqli_next_result|mysqli_num_fields|mysqli_num_rows|mysqli_options|mysqli_ping|mysqli_prepare|mysqli_query|mysqli_real_connect|mysqli_real_escape_string|mysqli_real_query|mysqli_reap_async_query|mysqli_refresh|mysqli_rollback|mysqli_select_db|mysqli_set_charset|mysqli_set_local_infile_default|mysqli_set_local_infile_handler|mysqli_sqlstate|mysqli_ssl_set|mysqli_stat|mysqli_stmt_init|mysqli_store_result|mysqli_thread_id|mysqli_thread_safe|mysqli_use_result|mysqli_warning_count"
#define MEMCACHED_KEY_STRING "set|setbykey|setmulti|setmultibykey|add|addbykey|replace|replacebykey|append|appendbykey|prepend|prependbykey|cas|casbykey|get|getbykey|getmulti|getmultibykey|getallkeys|delete|deletebykey|deletemulti|deletemultibykey|increment|incrementbykey|decrement|decrementbykey"
#define MEMCACHED_KEYS "set|setbykey|setmulti|setmultibykey|add|addbykey|replace|replacebykey|append|appendbykey|prepend|prependbykey|cas|casbykey|get|getbykey|getmulti|getmultibykey|getallkeys|delete|deletebykey|deletemulti|deletemultibykey|increment|incrementbykey|decrement|decrementbykey|getstats|ispersistent|ispristine|flush|flushbuffers|getdelayed|getdelayedbykey|fetch|fetchall|addserver|addservers|getoption|setoption|setoptions|getresultcode|getserverlist|resetserverlist|getversion|quit|setsaslauthdata|touch|touchbykey"

#define YAR_CLIENT_KEYS "__call"
#define YAR_SERVER_KEYS "__construct"

#define COMPONENTS_KEY  "component"
#define COMPONENTS_HTTP  "http"
#define COMPONENTS_REQUEST  "request"
#define COMPONENTS_PROCESS  "process"
#define COMPONENTS_ERROR  "error"
#define COMPONENTS_EXCEPTION  "exception"
#define COMPONENTS_CURL  "curl"
#define COMPONENTS_YAR  "yar"
#define COMPONENTS_YAR_CLIENT  "yar_client"
#define COMPONENTS_YAR_SERVER  "yar_server"
#define COMPONENTS_GRPC  "grpc"
#define COMPONENTS_DB  "db"
#define COMPONENTS_MYSQL  "mysql"
#define COMPONENTS_REDIS  "redis"
#define COMPONENTS_MEMCACHED  "memcached"
#define COMPONENTS_PREDIS  "predis"

#if (defined(unix) || defined(__unix__) || defined(__unix)) && !defined(__APPLE__)
#define PLATFORM_NAME "unix"
#elif defined(__linux__)
#define PLATFORM_NAME "linux"
#elif defined(__APPLE__) && defined(__MACH__)
#define PLATFORM_NAME "darwin"
#elif defined(__FreeBSD__)
#define PLATFORM_NAME "freebsd"
#else
#define PLATFORM_NAME ""
#endif

#ifdef ZEND_ENABLE_ZVAL_LONG64
#define PRId3264 PRId64
#else
#define PRId3264 PRId32
#endif

#ifdef PHP_WIN32
#define PHP_OPENTELEMETRY_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#define PHP_OPENTELEMETRY_API __attribute__ ((visibility("default")))
#else
#define PHP_OPENTELEMETRY_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

OPENTELEMETRY_END_EXTERN_C()

ZEND_BEGIN_MODULE_GLOBALS(opentelemetry)
  bool enable;
  bool cli_enable;
  bool debug;
  bool enable_exception;
  bool enable_error;
  bool enable_curl;
  bool enable_memcached;
  bool enable_redis;
  bool enable_mysql;
  bool enable_yar;
  bool enable_collect;
  char *environment;
  char *service_name;
  char *service_name_key;
  char *error_level;
  int sample_ratio_based;
  int max_time_consuming;
  char *log_path;
  char *grpc_endpoint;
  int grpc_max_message_size;
  int grpc_max_queue_length;
  int grpc_consumer;
  int grpc_timeout_milliseconds;
  Provider *provider;
  std::string ipv4;
  zval curl_header;
  bool is_started_cli_tracer;
ZEND_END_MODULE_GLOBALS(opentelemetry)
ZEND_EXTERN_MODULE_GLOBALS(opentelemetry)

#define OPENTELEMETRY_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(opentelemetry, v)
#if defined(ZTS) && defined(COMPILE_DL_OPENTELEMETRY)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif    /* PHP_OPENTELEMETRY_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

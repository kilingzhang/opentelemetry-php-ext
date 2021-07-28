//
// Created by kilingzhang on 2021/6/15.
//

#ifndef OPENTELEMETRY_UTILS_H
#define OPENTELEMETRY_UTILS_H

#include "php_opentelemetry.h"
#include <string>
#include <arpa/inet.h>
#include "zend_types.h"
#include "zend_smart_str.h"
#include "Zend/zend_smart_str.h"
#include <opentelemetry/proto/common/v1/common.pb.h>
#include <opentelemetry/proto/trace/v1/trace.pb.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::trace::v1;

/**
 *
 * @param message
 */
void log(const std::string &message);

/**
 *
 * @return
 */
bool is_has_provider();

/**
 *
 * @return
 */
bool is_started_cli_tracer();

/**
 *
 * @return
 */
bool is_enabled();

/**
 *
 * @return
 */
bool is_cli_enabled();

/**
 *
 * @return
 */
bool is_debug();

/**
 *
 * @return
 */
bool is_cli_sapi();

char *string2char(const std::string &str);

/**
 *
 * @param sockfd
 * @param saptr
 * @param salen
 * @param nsec
 * @return
 */
int connect_nonb(int sockfd, const struct sockaddr *saptr, socklen_t salen, int nsec);

/**
 *
 * @param message
 * @param unix_socket
 */
void opentelemetry_write_unix_socket(const std::string &message, char *unix_socket);

/**
 *
 * @param parameter
 * @return
 */
std::string opentelemetry_json_encode(zval *parameter);

std::string find_server_string(const char *key, size_t len);

zval *find_server_zval(const char *key, size_t len);

/**
 *
 * @return
 */
boost::uuids::uuid generate_trace_id();

/**
 *
 * @return
 */
boost::uuids::uuid generate_span_id();

/**
 *
 * @param ch
 * @param size
 * @return
 */
std::string to_hex(char *ch, int size);


/**
 *
 * @param hex
 * @return
 */
std::string HexDecode(const std::string &hex);

/**
 *
 * @param pre
 * @param str
 * @return
 */
bool starts_with(const char *pre, const char *str);

/**
 *
 * @return
 */
long get_unix_nanoseconds();

/**
 * 获取本机ip
 * @param eth_inf
 * @return
 */
std::string get_current_machine_ip(const char *eth_inf);

/**
 * 获取当前线程id
 * @return
 */
pid_t get_current_ppid();

/**
 *
 * @param str
 * @param search
 * @return
 */
bool is_contain(const std::string &str, char *search);

/**
 *
 * @param str
 * @param search
 * @return
 */
bool is_contain_const(const std::string &str, const char *search);

/**
 *
 * @param a
 * @param b
 * @return
 */
bool is_equal_const(const char *a, const char *b);

/**
 *
 * @param a
 * @param b
 * @return
 */
bool is_equal(const std::string &a, const char *b);

/**
 *
 * @param s
 * @return
 */
std::string trim(std::string s);

/**
 *
 * @param str
 * @param pattern
 * @return
 */
std::vector<std::string> split(const std::string &str, const std::string &pattern);

zval *read_object_property(zval *obj, const char *property, int parent);

std::string find_trace_generate_class_name(zend_string *class_name, zend_string *function_name, uint32_t fn_flags);

std::string find_trace_add_scope_name(zend_string *function_name, zend_class_entry *scope, uint32_t fn_flags);

std::string find_trace_caller(zend_execute_data *caller);

void set_string_attribute(KeyValue *attribute, const std::string &key, const std::string &value);

void set_int64_attribute(KeyValue *attribute, const std::string &key, int64_t value);

void set_bool_attribute(KeyValue *attribute, const std::string &key, bool value);

void set_span_ok(Span *span);

void set_span_error(Span *span, const std::string &message);

void okEnd(Span *span);

void errorEnd(Span *span, const std::string &message);

std::string formatTraceParentHeader(Span *span);

std::string formatTraceStateHeader(Span *span);
#endif //OPENTELEMETRY_UTILS_H

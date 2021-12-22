//
// Created by kilingzhang on 2021/6/15.
//

#include "php_opentelemetry.h"
#include "include/utils.h"
#include <include/hex.h>
#include "zend_types.h"
#include "zend_smart_str.h"

#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <fcntl.h>
#include <cerrno>
#include <curl/curl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <fstream>
#include <iostream>

#include <array>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/trace/v1/trace.pb.h"

using namespace opentelemetry::proto::trace::v1;
using namespace opentelemetry::proto::common::v1;

void log(const std::string &message) {
	std::string file = std::string(OPENTELEMETRY_G(log_path)) + "/opentelemetry.log";
	std::ofstream ofs;
	ofs.open(file, std::ofstream::out | std::ios::app);
	ofs << message << std::endl;
	ofs.close();
	file.shrink_to_fit();
}

bool is_receiver_udp() {
	return is_equal_const("udp", OPENTELEMETRY_G(receiver_type));
}

bool is_receiver_grpc() {
	return is_equal_const("grpc", OPENTELEMETRY_G(receiver_type));
}

bool is_has_provider() {
	return OPENTELEMETRY_G(provider) != nullptr;
}

bool is_started_cli_tracer() {
	return OPENTELEMETRY_G(is_started_cli_tracer);
}

bool is_enabled() {
	return OPENTELEMETRY_G(enable);
}

bool is_cli_enabled() {
	return OPENTELEMETRY_G(cli_enable);
}

bool is_debug() {
	return OPENTELEMETRY_G(debug);
}

bool is_cli_sapi() {
	return strcasecmp("cli", sapi_module.name) == 0;
}

char *string2char(const std::string &str) {
	char *ch = new char[str.length() + 1];
	strcpy(ch, str.c_str());
	return ch;
}

int connect_nonb(int sockfd, const struct sockaddr *saptr, socklen_t salen, int nsec) {
	int flags, n, error;
	socklen_t len;
	fd_set rset, wset;
	struct timeval tval{};

	// 设置 socket 为非阻塞
	if ((flags = fcntl(sockfd, F_GETFL, 0)) == -1) {
		perror("fcntl F_GETFL");
	}
	if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
		perror("fcntl F_SETFL");
	}

	error = 0;
	// 发起非阻塞 connect
	if ((n = connect(sockfd, saptr, salen)) < 0) {
		// EINPROGRESS 表示连接建立已启动但是尚未完成
		if (errno != EINPROGRESS) {
			return -1;
		}
	} else if (n == 0) {
		// 连接已经建立，当服务器处于客户端所在的主机时可能发生这种情况
		goto done;
	}

	FD_ZERO(&rset);
	FD_SET(sockfd, &rset);
	wset = rset;
	tval.tv_sec = nsec;
	tval.tv_usec = 0;

	// 等待套接字变为可读或可写，在 select 上等待连接完成
	if ((n = select(sockfd + 1, &rset, &wset, nullptr, nsec ? &tval : nullptr)) == 0) {
		// select 返回0，说明超时发生，需要关闭套接字，以防止已经启动的三次握手继续下去
		close(sockfd);
		errno = ETIMEDOUT;
		return -1;
	} else if (n == -1) {
		close(sockfd);
		perror("select");
		return -1;
	}

	if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset)) {
		len = sizeof(error);
		// 获取待处理错误，如果建立成功，error 为0；
		// 如果连接建立发生错误，该值就是对应错误的 errno 值
		if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
			// Berkeley 实现将在 error 中返回待处理错误，getsocket 本身返回 0
			// Solaris 实现将 getsocket 返回 -1，并把 errno 变量设置为待处理错误
			return -1;
		}
	} else {
		fprintf(stderr, "select error: socket not set");
	}

	done:

	// 关闭非阻塞状态
	if (fcntl(sockfd, F_SETFL, flags) == -1) {
		perror("fcntl");
	}

	if (error) {
		close(sockfd);
		errno = error;
		return -1;
	}

	return 0;
}

void opentelemetry_write_unix_socket(const std::string &message, char *unix_socket) {

	if (message.empty()) {
		php_error(E_WARNING, "[opentelemetry] socket message is empty .");
		return;
	}

	struct sockaddr_un un{};
	strcpy(un.sun_path, unix_socket);
	un.sun_family = AF_UNIX;
	int fd;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd >= 0) {
		int conn = connect_nonb(fd, (struct sockaddr *) &un, sizeof(un), 50000);
		if (conn >= 0) {
			write(fd, (message + "\n").c_str(), strlen((message + "\n").c_str()));
		} else {
			if (errno != 0) {
				php_error(E_WARNING, "[opentelemetry] failed to connect the sock error : %s .", strerror(errno));
				if (is_debug()) {
					log(message);
				}
			}
		}
		close(fd);
	} else {
		if (errno != 0) {
			php_error(E_WARNING, "[opentelemetry] failed to open the sock error : %s .", strerror(errno));
			if (is_debug()) {
				log(message);
			}
		}
	}
}

std::string opentelemetry_json_encode(zval *parameter) {
	std::string str;
	smart_str buf = {nullptr};
	zend_long options = 256;
#if PHP_VERSION_ID >= 70100
	if (php_json_encode(&buf, parameter, (int) options) != SUCCESS) {
		smart_str_free(&buf);
		return str;
	}
#else
	php_json_encode(&buf, parameter, (int) options);
#endif
	smart_str_0(&buf);
	if (buf.s != nullptr) {
		str = std::string(ZSTR_VAL(buf.s));
		smart_str_free(&buf);
	}
	return str;
}

std::string find_server_string(const char *key, size_t len) {
	zval *server_value;
	std::string value;
	server_value = find_server_zval(key, len);
	if (server_value) {
		value = Z_STRVAL_P(server_value);
	}
	return value;
}

zval *find_server_zval(const char *key, size_t len) {
	zval *carrier;
	zval *server_value;
	zend_bool jit_initialization = PG(auto_globals_jit);
	if (jit_initialization) {
		zend_string *server_str = zend_string_init("_SERVER", sizeof("_SERVER") - 1, 0);
		zend_is_auto_global(server_str);
		zend_string_release(server_str);
	}
	carrier = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SERVER"));
	server_value = zend_hash_str_find(Z_ARRVAL_P(carrier), key, len);
	return server_value;
}

zval *get_post_zval() {
	zend_bool jit_initialization = PG(auto_globals_jit);
	if (jit_initialization) {
		zend_string *str = zend_string_init("_POST", sizeof("_POST") - 1, 0);
		zend_is_auto_global(str);
		zend_string_release(str);
	}
	return zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_POST"));
}

boost::uuids::uuid generate_span_id() {
	return boost::uuids::random_generator()();
}

boost::uuids::uuid generate_trace_id() {
	return boost::uuids::random_generator()();
}

bool starts_with(const char *pre, const char *str) {
	size_t len_pre = strlen(pre),
		len_str = strlen(str);
	return len_str >= len_pre && memcmp(pre, str, len_pre) == 0;
}

long get_unix_nanoseconds() {
	auto ms = std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::system_clock::now().time_since_epoch());
	return ms.count();
}

// 获取本机ip
std::string get_current_machine_ip(const char *eth_inf) {
	int sd;
	struct sockaddr_in sin{};
	struct ifreq ifr{};

	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == sd) {
		return "";
	}

	strncpy(ifr.ifr_name, eth_inf, IFNAMSIZ);
	ifr.ifr_name[IFNAMSIZ - 1] = 0;

	if (ioctl(sd, SIOCGIFADDR, &ifr) < 0) {
		printf("ioctl error: %s\n", strerror(errno));
		close(sd);
		return "";
	}

	memcpy(&sin, &ifr.ifr_addr, sizeof(sin));

	close(sd);
	return std::string(inet_ntoa(sin.sin_addr));
}

static pid_t ppid = 0;

pid_t get_current_ppid() {
	if (is_cli_sapi()) {
		return getppid();
	} else {
		return ppid;
	}
}

bool is_contain(const std::string &str, char *search) {
	return strstr(search, str.c_str());
}

bool is_contain_const(const std::string &str, const char *search) {
	return strstr(search, str.c_str());
}

bool is_equal_const(const char *a, const char *b) {
	return strcmp(a, b) == 0;
}

bool is_equal(const std::string &a, const char *b) {
	return strcmp(a.c_str(), b) == 0;
}

std::string trim(std::string s) {
	if (s.empty()) {
		return s;
	}

	s.erase(0, s.find_first_not_of(' '));
	s.erase(s.find_last_not_of(' ') + 1);
	return s;
}

std::vector<std::string> split(const std::string &str, const std::string &pattern) {
	std::vector<std::string> res;
	if (str.empty()) {
		return res;
	}
	//在字符串末尾也加入分隔符，方便截取最后一段
	std::string basicString = str + pattern;
	size_t pos = basicString.find(pattern);

	while (pos != std::string::npos) {
		std::string temp = basicString.substr(0, pos);
		res.emplace_back(temp);
		//去掉已分割的字符串,在剩下的字符串中进行分割
		basicString = basicString.substr(pos + 1, basicString.size());
		pos = basicString.find(pattern);
	}

	return res;
}

std::vector<std::string> explode(const std::string &delimiter, const std::string &str) {
	std::vector<std::string> arr;
	unsigned long length = str.length();
	unsigned long delLength = delimiter.length();
	if (delLength == 0) {
		return arr;
	}

	unsigned long i = 0;
	unsigned long k = 0;
	while (i < length) {
		int j = 0;
		while (i + j < length && j < delLength && str[i + j] == delimiter[j])
			j++;
		if (j == delLength)//found delimiter
		{
			arr.emplace_back(str.substr(k, i - k));
			i += delLength;
			k = i;
		} else {
			i++;
		}
	}
	arr.emplace_back(str.substr(k, i - k));
	return arr;
}

zval *read_object_property(zval *obj, const char *property, int parent) {
	if (Z_TYPE_P(obj) == IS_OBJECT) {
		zend_object *object = obj->value.obj;
		zend_class_entry *ce;

		if (parent == 0) {
			ce = object->ce;
		} else {
			ce = object->ce->parent;
		}

#if PHP_VERSION_ID < 80000
		return zend_read_property(ce, obj, property, strlen(property), 0, nullptr);
#else
		return zend_read_property(ce, object, property, strlen(property), 0, nullptr);
#endif
	}
	return nullptr;
}

/**
 * Given a class name and a function name, return a new string that represents
 * the function name. Note that this zend_string should be released when
 * finished.
 */
std::string find_trace_generate_class_name(zend_string *class_name, zend_string *function_name, uint32_t fn_flags) {
	std::string name;
	name = name.append(class_name->val);
	if (fn_flags & ZEND_ACC_STATIC) {
		name = name.append("::");
	} else {
		name = name.append("->");
	}
	name = name.append(function_name->val);
	return name;
}

/* Prepend the name of the scope class to the function name */
std::string find_trace_add_scope_name(zend_string *function_name, zend_class_entry *scope, uint32_t fn_flags) {
	std::string name;
	if (!function_name) {
		return name;
	}
	if (scope) {
		name = find_trace_generate_class_name(scope->name, function_name, fn_flags);
	} else {
		name = std::string(function_name->val);
	}
	return name;
}

/* Prepend the name of the scope class to the function name */
std::string find_code_stacktrace(zend_execute_data *caller) {

	std::string name;
	std::string function_name;
	if (caller == nullptr || caller->func == nullptr) {
		return name;
	}
	if (caller->func->common.function_name) {
		function_name = find_trace_add_scope_name(caller->func->common.function_name, caller->func->common.scope,
		                                          caller->func->common.fn_flags);
	} else if (caller->func->internal_function.function_name) {
		function_name = find_trace_add_scope_name(caller->func->internal_function.function_name,
		                                          caller->func->internal_function.scope,
		                                          caller->func->internal_function.fn_flags);
	}
	caller = caller->prev_execute_data;
	if (caller != nullptr && caller->func) {
		name = find_code_stacktrace(caller);
		if (!name.empty()) {
			name = name.append("\\");
		}
	}

	if (!function_name.empty()) {
		name = name.append(function_name);
	}
	return name;
}

void set_string_attribute(KeyValue *attribute, const std::string &key, const std::string &value) {
	attribute->set_key(key);
	auto anyValue = new AnyValue();
	anyValue->set_string_value(value);
	attribute->set_allocated_value(anyValue);
}

void set_double_attribute(KeyValue *attribute, const std::string &key, const std::string &value) {
	attribute->set_key(key);
	auto anyValue = new AnyValue();
	anyValue->set_string_value(value);
	attribute->set_allocated_value(anyValue);
}

void set_int64_attribute(KeyValue *attribute, const std::string &key, int64_t value) {
	attribute->set_key(key);
	auto anyValue = new AnyValue();
	anyValue->set_int_value(value);
	attribute->set_allocated_value(anyValue);
}

void set_double_attribute(KeyValue *attribute, const std::string &key, double value) {
	attribute->set_key(key);
	auto anyValue = new AnyValue();
	anyValue->set_double_value(value);
	attribute->set_allocated_value(anyValue);
}

void set_bool_attribute(KeyValue *attribute, const std::string &key, bool value) {
	attribute->set_key(key);
	auto anyValue = new AnyValue();
	anyValue->set_bool_value(value);
	attribute->set_allocated_value(anyValue);
}

std::string traceId(const Span &span) {
	return Hex::encode(reinterpret_cast<const uint8_t *>(span.trace_id().data()), 16);
}

std::string spanId(const Span &span) {
	return Hex::encode(reinterpret_cast<const uint8_t *>(span.span_id().data()), 8);
}
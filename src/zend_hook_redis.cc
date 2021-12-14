//
// Created by kilingzhang on 2021/6/24.
//

#include "php_opentelemetry.h"
#include "include/zend_hook_redis.h"
#include "include/utils.h"
#include "zend_types.h"

#include "opentelemetry/proto/trace/v1/trace.pb.h"

using namespace opentelemetry::proto::trace::v1;

std::vector<std::string> redisKeysCommands;

void opentelemetry_redis_handler(INTERNAL_FUNCTION_PARAMETERS) {
	zend_function *zf = execute_data->func;
	std::string name;
	std::string class_name;
	std::string function_name;
	std::string code_stacktrace;

	if (zf->internal_function.scope != nullptr && zf->internal_function.scope->name != nullptr) {
		class_name = std::string(ZSTR_VAL(zf->internal_function.scope->name));
	}
	if (zf->internal_function.function_name != nullptr) {
		function_name = std::string(ZSTR_VAL(zf->internal_function.function_name));
	}
	name = find_trace_add_scope_name(zf->internal_function.function_name, zf->internal_function.scope,
	                                 zf->internal_function.fn_flags);

	std::string cmd = function_name;
	std::transform(function_name.begin(), function_name.end(), cmd.begin(), ::tolower);
	Span *span;
	if (is_has_provider()) {
		std::string parentId = OPENTELEMETRY_G(provider)->latestSpan().span_id();
		span = OPENTELEMETRY_G(provider)->createSpan(name, Span_SpanKind_SPAN_KIND_CLIENT);
		set_string_attribute(span->add_attributes(), "db.system", COMPONENTS_REDIS);
		set_string_attribute(span->add_attributes(), "db.operation", cmd);
		span->set_parent_span_id(parentId);
		zend_execute_data *caller = execute_data->prev_execute_data;
		if (caller != nullptr && caller->func) {
			code_stacktrace = find_code_stacktrace(caller);
			if (!code_stacktrace.empty()) {
				set_string_attribute(span->add_attributes(), "code.stacktrace", code_stacktrace);
			}
		}

		set_string_attribute(span->add_attributes(), "net.transport", "IP.TCP");

		uint32_t arg_count = ZEND_CALL_NUM_ARGS(execute_data);
		std::string command = cmd;
		command = command.append(" ");
		if (std::find(redisKeysCommands.begin(), redisKeysCommands.end(), cmd) != redisKeysCommands.end()) {

			zval *self = &(execute_data->This);
			zval zval_host;
			zval zval_port;
			zend_call_method(self, Z_OBJCE_P(self), nullptr, ZEND_STRL("gethost"), &zval_host, 0, nullptr, nullptr);
			zend_call_method(self, Z_OBJCE_P(self), nullptr, ZEND_STRL("getport"), &zval_port, 0, nullptr, nullptr);

			if (!Z_ISUNDEF(zval_host) && !Z_ISUNDEF(zval_port) && Z_TYPE(zval_host) == IS_STRING &&
				Z_TYPE(zval_port) == IS_LONG) {
				std::string host = ZSTR_VAL(Z_STR(zval_host));
				long port = Z_LVAL(zval_port);

				if (inet_addr(host.c_str()) != INADDR_NONE) {
					set_string_attribute(span->add_attributes(), "net.peer.ip", host);
				} else {
					set_string_attribute(span->add_attributes(), "net.peer.name", host);
				}
				set_int64_attribute(span->add_attributes(), "net.peer.port", port);

				host.shrink_to_fit();
			}

			if (!Z_ISUNDEF(zval_host)) {
				zval_dtor(&zval_host);
			}

			if (!Z_ISUNDEF(zval_port)) {
				zval_dtor(&zval_port);
			}
		}

		int i;
		for (i = 1; i < arg_count + 1; ++i) {
			zval str_p;
			zval *p = ZEND_CALL_ARG(execute_data, i);
			if (Z_TYPE_P(p) == IS_ARRAY) {
				command = command.append(opentelemetry_json_encode(p));
				continue;
			}

			ZVAL_COPY(&str_p, p);
			if (Z_TYPE_P(&str_p) != IS_STRING) {
				convert_to_string(&str_p);
			}

			if (i == 1 && !is_equal(cmd, "connect")) {
				set_string_attribute(span->add_attributes(), "db.cache.key", Z_STRVAL_P(&str_p));
			} else if (i == 1 && is_equal(cmd, "connect")) {
				if (inet_addr(Z_STRVAL_P(&str_p)) != INADDR_NONE) {
					set_string_attribute(span->add_attributes(), "net.peer.ip", Z_STRVAL_P(&str_p));
				} else {
					set_string_attribute(span->add_attributes(), "net.peer.name", Z_STRVAL_P(&str_p));
				}
			} else if (i == 2 && is_equal(cmd, "connect")) {
				if (!Z_ISUNDEF_P(p)) {
					set_int64_attribute(span->add_attributes(), "net.peer.port", std::stoi(Z_STRVAL_P(&str_p)));
				}
			}

			char *tmp = zend_str_tolower_dup(Z_STRVAL_P(&str_p), Z_STRLEN_P(&str_p));
			command = command.append(tmp);
			command = command.append(" ");
			efree(tmp);
			zval_dtor(&str_p);
		}

		// store command to tags
		if (!command.empty()) {
			std::transform(command.begin(), command.end(), command.begin(), ::tolower);
			command = trim(command);
			set_string_attribute(span->add_attributes(), "db.statement", command);
		}

		command.shrink_to_fit();
	}

	if (class_name == "Redis" && !opentelemetry_redis_original_handler_map.empty() &&
		opentelemetry_redis_original_handler_map.find(cmd) !=
			opentelemetry_redis_original_handler_map.end()) {
		opentelemetry_redis_original_handler_map[cmd](INTERNAL_FUNCTION_PARAM_PASSTHRU);
	} else if (class_name == "RedisCluster" && !opentelemetry_rediscluster_original_handler_map.empty() &&
		opentelemetry_rediscluster_original_handler_map.find(cmd) !=
			opentelemetry_rediscluster_original_handler_map.end()) {
		opentelemetry_rediscluster_original_handler_map[cmd](INTERNAL_FUNCTION_PARAM_PASSTHRU);
	}

	if (is_has_provider()) {
		if (OPENTELEMETRY_G(provider)->isRedisException()) {
			Provider::errorEnd(span, OPENTELEMETRY_G(provider)->getRedisException());
			OPENTELEMETRY_G(provider)->clearRedisException();
		} else {
			Provider::okEnd(span);
		}
	}

	cmd.clear();
	cmd.shrink_to_fit();
	name.shrink_to_fit();
	class_name.shrink_to_fit();
	function_name.shrink_to_fit();
	code_stacktrace.shrink_to_fit();
}

void register_zend_hook_redis() {
	redisKeysCommands = {"connect", "dump", "exists", "expire", "expireat", "move", "persist", "pexpire", "pexpireat",
		"pttl", "rename", "renamenx", "sort", "ttl", "type", "append", "bitcount", "bitfield", "decr",
		"decrby", "get", "getbit", "getrange", "getset", "incr", "incrby", "incrbyfloat", "psetex",
		"set", "setbit", "setex", "setnx", "setrange", "strlen", "bitop", "hdel", "hexists", "hget",
		"hgetall", "hincrby", "hincrbyfloat", "hkeys", "hlen", "hmget", "hmset", "hscan", "hset",
		"hsetnx", "hvals", "hstrlen", "lindex", "linsert", "llen", "lpop", "lpush", "lpushx", "lrange",
		"lrem", "lset", "ltrim", "rpop", "rpush", "rpushx", "sadd", "scard", "sismember", "smembers",
		"spop", "srandmember", "srem", "sscan", "zadd", "zcard", "zcount", "zincrby", "zrange",
		"zrangebyscore", "zrank", "zrem", "zremrangebyrank", "zremrangebyscore", "zrevrange",
		"zrevrangebyscore", "zrevrank", "zscore", "zscan", "zrangebylex", "zrevrangebylex",
		"zremrangebylex", "zlexcount", "pfadd", "watch", "geoadd", "geohash", "geopos", "geodist",
		"georadius", "georadiusbymember"};
	zend_class_entry *old_class;
	zend_function *old_function;
	if ((old_class = OPENTELEMETRY_OLD_CN("redis")) != nullptr) {
		for (const auto &item: redisKeysCommands) {
			if ((old_function = OPENTELEMETRY_OLD_FN_TABLE(
				&old_class->function_table, item.c_str())) !=
				nullptr) {
				opentelemetry_redis_original_handler_map[item.c_str()] = old_function->internal_function.handler;
				old_function->internal_function.handler = opentelemetry_redis_handler;
			}
		}
	}

	if ((old_class = OPENTELEMETRY_OLD_CN("rediscluster")) != nullptr) {
		for (const auto &item: redisKeysCommands) {
			if ((old_function = OPENTELEMETRY_OLD_FN_TABLE(
				&old_class->function_table, item.c_str())) !=
				nullptr) {
				opentelemetry_rediscluster_original_handler_map[item.c_str()] = old_function->internal_function.handler;
				old_function->internal_function.handler = opentelemetry_redis_handler;
			}
		}
	}
}

void unregister_zend_hook_redis() {
	zend_class_entry *old_class;
	zend_function *old_function;
	if ((old_class = OPENTELEMETRY_OLD_CN("redis")) != nullptr) {
		for (const auto &item: redisKeysCommands) {
			if ((old_function = OPENTELEMETRY_OLD_FN_TABLE(
				&old_class->function_table, item.c_str())) !=
				nullptr) {
				old_function->internal_function.handler = opentelemetry_redis_original_handler_map[item.c_str()];
			}
		}
	}

	if ((old_class = OPENTELEMETRY_OLD_CN("rediscluster")) != nullptr) {
		for (const auto &item: redisKeysCommands) {
			if ((old_function = OPENTELEMETRY_OLD_FN_TABLE(
				&old_class->function_table, item.c_str())) !=
				nullptr) {
				old_function->internal_function.handler = opentelemetry_rediscluster_original_handler_map[item.c_str()];
			}
		}
	}
	redisKeysCommands.clear();
}
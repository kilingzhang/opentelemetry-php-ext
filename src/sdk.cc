//
// Created by kilingzhang on 2021/6/16.
//

#include "php_opentelemetry.h"
#include "include/sdk.h"
#include "include/utils.h"
#include "include/core.h"
#include "Zend/zend_hash.h"

#include <random>

#include "opentelemetry/proto/trace/v1/trace.pb.h"

using namespace opentelemetry::proto::trace::v1;

/**
 *
 * @param execute_data
 * @param return_value
 */
PHP_FUNCTION (opentelemetry_start_cli_tracer) {

	//非cli模式 或 未开启cli模块收集
	if (!is_cli_sapi() || !is_cli_enabled() || is_started_cli_tracer()) {
		RETURN_FALSE;
	}

	char *traceparent = nullptr;
	size_t traceparent_len;
	char *tracestate = nullptr;
	size_t tracestate_len;
	long kind = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|ssl",
	                          &traceparent, &traceparent_len, &tracestate, &tracestate_len, &kind) == FAILURE) {
		RETURN_FALSE;
	}

	OPENTELEMETRY_G(is_started_cli_tracer) = true;

	if (traceparent && tracestate) {
		start_tracer(traceparent, tracestate, (Span_SpanKind) kind);
	} else if (traceparent && !tracestate) {
		start_tracer(traceparent, "", (Span_SpanKind) kind);
	} else if (!traceparent && tracestate) {
		start_tracer("", tracestate, (Span_SpanKind) kind);
	} else {
		start_tracer("", "", (Span_SpanKind) kind);
	}
	RETURN_TRUE;
}

/**
 *
 * @param execute_data
 * @param return_value
 */
PHP_FUNCTION (opentelemetry_shutdown_cli_tracer) {

	//非cli模式 或 未开启cli模块收集
	if (!is_cli_sapi() || !is_cli_enabled() || !is_started_cli_tracer()) {
		RETURN_FALSE;
	}

	shutdown_tracer();
	RETURN_TRUE;
}

/**
 *
 * @param execute_data
 * @param return_value
 */
PHP_FUNCTION (opentelemetry_get_traceparent) {
	if (is_has_provider()) {
		std::string traceparent = OPENTELEMETRY_G(provider)->formatTraceParentHeader(OPENTELEMETRY_G(provider)->firstOneSpan());
		RETURN_STRING(string2char(traceparent));
	}
	RETURN_STRING("");
}

/**
 *
 * @param execute_data
 * @param return_value
 */
PHP_FUNCTION (opentelemetry_get_tracestate) {
	if (is_has_provider()) {
		std::string tracestate = Provider::formatTraceStateHeader();
		RETURN_STRING(string2char(tracestate));
	}
	RETURN_STRING("");
}

PHP_FUNCTION (opentelemetry_add_tracestate) {

	if (is_has_provider()) {
		char *key = nullptr;
		size_t key_len;
		char *value = nullptr;
		size_t value_len;

		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|ss",
		                          &key, &key_len, &value, &value_len) == FAILURE) {
			RETURN_FALSE;
		}

		OPENTELEMETRY_G(provider)->addTraceStates(key, value);

		RETURN_TRUE;
	}
	RETURN_FALSE;
}

/**
 *
 * @param execute_data
 * @param return_value
 */
PHP_FUNCTION (opentelemetry_set_sample_ratio_based) {
	if (is_has_provider()) {
		long ratio_based;
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &ratio_based) == SUCCESS) {
			if (ratio_based == 0) {
				OPENTELEMETRY_G(provider)->setSampled(false);
			} else if (ratio_based == 1) {
				OPENTELEMETRY_G(provider)->setSampled(true);
			} else {
				std::string traceparent = find_server_string("HTTP_TRACEPARENT", sizeof("HTTP_TRACEPARENT") - 1);
				if ((traceparent.empty() && OPENTELEMETRY_G(provider)->count() % OPENTELEMETRY_G(sample_ratio_based) == 0) ||
					(!traceparent.empty() && ratio_based == 1)) {
					OPENTELEMETRY_G(provider)->setSampled(true);
				}
			}

		}
	}
}

/**
 *
 * @param execute_data
 * @param return_value
 */
PHP_FUNCTION (opentelemetry_get_service_name) {
	RETURN_STRING(OPENTELEMETRY_G(service_name));
}

/**
 *
 * @param execute_data
 * @param return_value
 */
PHP_FUNCTION (opentelemetry_get_service_ip) {
	RETURN_STRING(OPENTELEMETRY_G(ipv4).c_str());
}

/**
 *
 * @param execute_data
 * @param return_value
 */
PHP_FUNCTION (opentelemetry_get_ppid) {
	RETURN_LONG(get_current_ppid());
}

/**
 *
 * @param execute_data
 * @param return_value
 */
PHP_FUNCTION (opentelemetry_get_unix_nano) {
	RETURN_LONG(get_unix_nanoseconds());
}

/**
 *
 * @param execute_data
 * @param return_value
 */
PHP_FUNCTION (opentelemetry_get_environment) {
	RETURN_STRING(OPENTELEMETRY_G(environment));
}

/**
 *
 * @param execute_data
 * @param return_value
 */
PHP_FUNCTION (opentelemetry_add_span) {

	if (is_has_provider()) {

		char *name = nullptr;
		size_t name_len;
		long kind;
		long start_time_unix_nano;
		long status_code;
		char *status_message = nullptr;
		size_t status_message_len;
		zval *attributes;

		//name, kind, start_time_unix_nano, attributes
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slllsa",
		                          &name, &name_len, &kind, &start_time_unix_nano, &status_code, &status_message, &status_message_len, &attributes) == FAILURE) {
			RETURN_FALSE;
		}

		std::string parentId = OPENTELEMETRY_G(provider)->firstOneSpan()->span_id();
		auto span = OPENTELEMETRY_G(provider)->createSpan(name, Span_SpanKind(int(kind)));
		span->set_parent_span_id(parentId);
		span->set_start_time_unix_nano(start_time_unix_nano);

		zval *attribute;
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(attributes), attribute)
				{
					zval *key = zend_hash_str_find(Z_ARRVAL_P(attribute), ZEND_STRL("key"));
					zval *value = zend_hash_str_find(Z_ARRVAL_P(attribute), ZEND_STRL("value"));
					if (Z_TYPE_P(value) == IS_ARRAY) {
						set_string_attribute(span->add_attributes(), Z_STRVAL_P(key), opentelemetry_json_encode(value));
					} else if (Z_TYPE_P(value) == IS_STRING) {
						set_string_attribute(span->add_attributes(), Z_STRVAL_P(key), Z_STRVAL_P(value));
					} else {
						zval str_p;
						ZVAL_COPY(&str_p, value);
						convert_to_string(&str_p);
						set_string_attribute(span->add_attributes(), Z_STRVAL_P(key), Z_STRVAL_P(&str_p));
						zval_dtor(&str_p);
					}
				}
		ZEND_HASH_FOREACH_END();

		if (Status_StatusCode(int(status_code)) == Status_StatusCode_STATUS_CODE_ERROR) {
			Provider::errorEnd(span, status_message);
		} else {
			Provider::okEnd(span);
		}

		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_FUNCTION (opentelemetry_add_resource_attribute) {
	if (is_has_provider()) {

		char *key = nullptr;
		size_t key_len;
		char *value = nullptr;
		size_t value_len;

		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss",
		                          &key, &key_len, &value, &value_len) == FAILURE) {
			RETURN_FALSE;
		}

		set_string_attribute(OPENTELEMETRY_G(provider)->getResource()->add_attributes(), key, value);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_FUNCTION (opentelemetry_add_attribute) {
	if (is_has_provider()) {

		char *key = nullptr;
		size_t key_len;
		char *value = nullptr;
		size_t value_len;

		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss",
		                          &key, &key_len, &value, &value_len) == FAILURE) {
			RETURN_FALSE;
		}

		set_string_attribute(OPENTELEMETRY_G(provider)->firstOneSpan()->add_attributes(), key, value);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_FUNCTION (opentelemetry_add_event) {

	if (is_has_provider()) {

		char *name = nullptr;
		size_t name_len;
		zval *attributes;

		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa",
		                          &name, &name_len, &attributes) == FAILURE) {
			RETURN_FALSE;
		}

		auto event = OPENTELEMETRY_G(provider)->firstOneSpan()->add_events();
		event->set_name(name);
		event->set_time_unix_nano(get_unix_nanoseconds());

		zend_string *key;
		zval *entry_value;
		ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(attributes), key, entry_value)
				{
					if (key) { /* HASH_KEY_IS_STRING */
						if (Z_TYPE_P(entry_value) == IS_ARRAY) {
							set_string_attribute(event->add_attributes(), key->val, opentelemetry_json_encode(entry_value));
						} else if (Z_TYPE_P(entry_value) == IS_STRING) {
							set_string_attribute(event->add_attributes(), key->val, Z_STRVAL_P(entry_value));
						} else {
							zval str_p;
							ZVAL_COPY(&str_p, entry_value);
							convert_to_string(&str_p);
							set_string_attribute(event->add_attributes(), key->val, Z_STRVAL_P(&str_p));
							zval_dtor(&str_p);
						}
					}
				}ZEND_HASH_FOREACH_END();

		RETURN_TRUE;
	}
	RETURN_FALSE;
}
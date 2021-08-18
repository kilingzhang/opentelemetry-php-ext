//
// Created by kilingzhang on 2021/6/16.
//

#include "php_opentelemetry.h"
#include "include/sdk.h"
#include "include/utils.h"
#include "include/core.h"

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
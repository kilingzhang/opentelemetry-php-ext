//
// Created by kilingzhang on 2021/6/24.
//

#include "php_opentelemetry.h"
#include "include/zend_hook_curl.h"
#include "include/utils.h"
#include "zend_types.h"
#include <ext/standard/url.h>

#include <curl/curl.h>

#include "opentelemetry/proto/trace/v1/trace.pb.h"

using namespace opentelemetry::proto::trace::v1;

#if PHP_VERSION_ID >= 80000
#include "ext/curl/php_curl.h"
#endif

std::map<int, std::string> error_maps;
void opentelemetry_curl_exec_handler(INTERNAL_FUNCTION_PARAMETERS) {

	if (!is_has_provider()) {
		opentelemetry_original_curl_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU);
		return;
	}

	zval *zid;

#if PHP_VERSION_ID >= 80000
	ZEND_PARSE_PARAMETERS_START(1, 1)
			Z_PARAM_OBJECT_OF_CLASS(zid, curl_ce)
	ZEND_PARSE_PARAMETERS_END();
	zend_ulong cid = Z_OBJ_HANDLE_P(zid);
#else
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &zid) == FAILURE) {
		return;
	}
	zend_ulong cid = Z_RES_HANDLE_P(zid);
#endif

	int is_record = 0;

	zval func;
	zval args[1];
	zval url_info;
	ZVAL_COPY(&args[0], zid);
	ZVAL_STRING(&func, "curl_getinfo");
	call_user_function(CG(function_table), nullptr, &func, &url_info, 1, args);
	zval_dtor(&func);
	zval_dtor(&args[0]);

	// check
	php_url *url_parse = nullptr;
	zval *z_url = zend_hash_str_find(Z_ARRVAL(url_info), ZEND_STRL("url"));
	char *url_str = Z_STRVAL_P(z_url);
	if (strlen(url_str) > 0 && (starts_with("http://", url_str) || starts_with("https://", url_str))) {
		url_parse = php_url_parse(url_str);
		if (url_parse != nullptr && url_parse->scheme != nullptr && url_parse->host != nullptr) {
			is_record = 1;
		}
	}

	Span *span;
	// set header
	int isEMalloc = 0;
	zval *option;
	option = zend_hash_index_find(Z_ARRVAL_P(&OPENTELEMETRY_G(curl_header)), cid);
	if (is_record) {
		if (option == nullptr) {
			option = (zval *) emalloc(sizeof(zval));
			bzero(option, sizeof(zval));
			array_init(option);
			isEMalloc = 1;
		}

		// for php7.3.0+
#if PHP_VERSION_ID >= 70300
		char *php_url_scheme = ZSTR_VAL(url_parse->scheme);
		char *php_url_host = ZSTR_VAL(url_parse->host);
		char *php_url_path = url_parse->path != nullptr ? ZSTR_VAL(url_parse->path) : nullptr;
		char *php_url_query = ZSTR_VAL(url_parse->query);

#else
		char *php_url_scheme = url_parse->scheme;
		char *php_url_host = url_parse->host;
		char *php_url_path = url_parse->path;
		char *php_url_query = url_parse->query;
#endif

		std::string parentId = OPENTELEMETRY_G(provider)->firstOneSpan()->span_id();
		span = OPENTELEMETRY_G(provider)->createSpan(php_url_path == nullptr ? "/" : php_url_path, Span_SpanKind_SPAN_KIND_CLIENT);
		span->set_parent_span_id(parentId);
		//TODO http.method
		set_string_attribute(span->add_attributes(), "http.scheme", php_url_scheme == nullptr ? "" : php_url_scheme);
		set_string_attribute(span->add_attributes(), "http.host", php_url_host == nullptr ? "" : php_url_host);
		set_string_attribute(span->add_attributes(), "http.target", php_url_path == nullptr ? "" : (php_url_path + (php_url_query == nullptr ? "" : "?" + std::string(php_url_query))));

		std::string traceparent = OPENTELEMETRY_G(provider)->formatTraceParentHeader(span);
		add_next_index_string(option, ("traceparent: " + traceparent).c_str());
		std::string baggage = OPENTELEMETRY_G(provider)->formatBaggageHeader();
		add_next_index_string(option, ("baggage: " + baggage).c_str());

		set_string_attribute(span->add_attributes(), "http.header", opentelemetry_json_encode(option));

		traceparent.shrink_to_fit();
		baggage.shrink_to_fit();

		zval argv[3];
		zval ret;
		ZVAL_STRING(&func, "curl_setopt");
		ZVAL_COPY(&argv[0], zid);
		ZVAL_LONG(&argv[1], OPENTELEMETRY_CURLOPT_HTTPHEADER);
		ZVAL_COPY(&argv[2], option);
		call_user_function(CG(function_table), nullptr, &func, &ret, 3, argv);
		zval_dtor(&func);
		zval_dtor(&ret);
		zval_dtor(&argv[0]);
		zval_dtor(&argv[1]);
		zval_dtor(&argv[2]);
		if (isEMalloc) {
			zval_dtor(option);
			efree(option);
		}
	}

	opentelemetry_original_curl_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU);

	// record
	if (is_record == 1) {

		zend_execute_data *caller = execute_data->prev_execute_data;
		if (caller != nullptr && caller->func) {
			std::string code_stacktrace = find_code_stacktrace(caller);
			if (!code_stacktrace.empty()) {
				set_string_attribute(span->add_attributes(), "code.stacktrace", code_stacktrace);
			}
			code_stacktrace.shrink_to_fit();
		}

		// get response
		zval url_response;
		ZVAL_COPY(&args[0], zid);
		ZVAL_STRING(&func, "curl_getinfo");
		call_user_function(CG(function_table), nullptr, &func, &url_response, 1, args);
		zval_dtor(&func);
		zval_dtor(&args[0]);

		zval *response_http_code, *response_primary_ip, *response_total_time, *response_name_lookup_time, *response_connect_time, *response_pre_transfer_time, *response_start_transfer_time, *response_redirect_time;
		response_http_code = zend_hash_str_find(Z_ARRVAL(url_response), ZEND_STRL("http_code"));
		set_int64_attribute(span->add_attributes(), "http.status_code", Z_LVAL_P(response_http_code));
		response_primary_ip = zend_hash_str_find(Z_ARRVAL(url_response), ZEND_STRL("primary_ip"));
		set_string_attribute(span->add_attributes(), "net.peer.ip", Z_STR_P(response_primary_ip)->val);

		//总共的传输时间
		response_total_time = zend_hash_str_find(Z_ARRVAL(url_response), ZEND_STRL("total_time"));
		set_double_attribute(span->add_attributes(), "http.response_total_time", Z_DVAL_P(response_total_time));
		//直到DNS解析完成时间
		response_name_lookup_time = zend_hash_str_find(Z_ARRVAL(url_response), ZEND_STRL("namelookup_time"));
		set_double_attribute(span->add_attributes(), "http.response_name_lookup_time", Z_DVAL_P(response_name_lookup_time));
		//建立连接时间
		response_connect_time = zend_hash_str_find(Z_ARRVAL(url_response), ZEND_STRL("connect_time"));
		set_double_attribute(span->add_attributes(), "http.response_connect_time", Z_DVAL_P(response_connect_time));
		//传输前耗时
		response_pre_transfer_time = zend_hash_str_find(Z_ARRVAL(url_response), ZEND_STRL("pretransfer_time"));
		set_double_attribute(span->add_attributes(), "http.response_pre_transfer_time", Z_DVAL_P(response_pre_transfer_time));
		//开始传输
		response_start_transfer_time = zend_hash_str_find(Z_ARRVAL(url_response), ZEND_STRL("starttransfer_time"));
		set_double_attribute(span->add_attributes(), "http.response_start_transfer_time", Z_DVAL_P(response_start_transfer_time));
		//重定向时间
		response_redirect_time = zend_hash_str_find(Z_ARRVAL(url_response), ZEND_STRL("redirect_time"));
		set_double_attribute(span->add_attributes(), "http.response_redirect_time", Z_DVAL_P(response_redirect_time));

		if (Z_LVAL_P(response_http_code) == 0) {
			// get errors
			zval curl_error;
			ZVAL_COPY(&args[0], zid);
			ZVAL_STRING(&func, "curl_error");
			call_user_function(CG(function_table), nullptr, &func, &curl_error, 1, args);

			if (is_equal_const(Z_STRVAL(curl_error), "")) {
				ZVAL_STRING(&func, "curl_errno");
				call_user_function(CG(function_table), nullptr, &func, &curl_error, 1, args);
				int error = Z_LVAL(curl_error);
				if (error_maps.count(error) > 0) {
					Provider::errorEnd(span, error_maps[error]);
				} else {
					Provider::errorEnd(span, "CURLE_OTHER_CODE:" + std::to_string(error));
				}
			} else {
				Provider::errorEnd(span, Z_STRVAL(curl_error));
			}
			zval_dtor(&func);
			zval_dtor(&args[0]);
			zval_dtor(&curl_error);
		} else if (Z_LVAL_P(response_http_code) >= 400) {
			Provider::errorEnd(span, Z_STR_P(return_value)->val);
		}
		zval_dtor(&url_response);

		Provider::okEnd(span);
	}

	zval_dtor(&url_info);
	if (url_parse != nullptr) {
		php_url_free(url_parse);
	}

}

void opentelemetry_curl_setopt_handler(INTERNAL_FUNCTION_PARAMETERS) {

	if (!is_has_provider()) {
		opentelemetry_original_curl_setopt(INTERNAL_FUNCTION_PARAM_PASSTHRU);
		return;
	}

	zval *zid, *zvalue;
	zend_long options;
#if PHP_VERSION_ID >= 80000
	ZEND_PARSE_PARAMETERS_START(3, 3)
			Z_PARAM_OBJECT_OF_CLASS(zid, curl_ce)
			Z_PARAM_LONG(options)
			Z_PARAM_ZVAL(zvalue)
	ZEND_PARSE_PARAMETERS_END();
	zend_ulong cid = Z_OBJ_HANDLE_P(zid);
#else
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rlz", &zid, &options, &zvalue) == FAILURE) {
		return;
	}
	zend_ulong cid = Z_RES_HANDLE_P(zid);
#endif

	if (OPENTELEMETRY_CURLOPT_HTTPHEADER == options) {
		zval *header = ZEND_CALL_ARG(execute_data, 2);
		header->value.lval = CURLOPT_HTTPHEADER;
	} else if (CURLOPT_HTTPHEADER == options && Z_TYPE_P(zvalue) == IS_ARRAY) {
		zval dup_header;
		ZVAL_DUP(&dup_header, zvalue);
		add_index_zval(&OPENTELEMETRY_G(curl_header), cid, &dup_header);
	}

	opentelemetry_original_curl_setopt(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

void opentelemetry_curl_setopt_array_handler(INTERNAL_FUNCTION_PARAMETERS) {

	if (!is_has_provider()) {
		opentelemetry_original_curl_setopt_array(INTERNAL_FUNCTION_PARAM_PASSTHRU);
		return;
	}

	zval *zid, *arr;
#if PHP_VERSION_ID >= 80000
	ZEND_PARSE_PARAMETERS_START(2, 2)
			Z_PARAM_OBJECT_OF_CLASS(zid, curl_ce)
			Z_PARAM_ARRAY(arr)
	ZEND_PARSE_PARAMETERS_END();
	zend_ulong cid = Z_OBJ_HANDLE_P(zid);
#else
	ZEND_PARSE_PARAMETERS_START(2, 2)
			Z_PARAM_RESOURCE(zid)
			Z_PARAM_ARRAY(arr)
	ZEND_PARSE_PARAMETERS_END();
	zend_ulong cid = Z_RES_HANDLE_P(zid);
#endif

	zval *http_header = zend_hash_index_find(Z_ARRVAL_P(arr), CURLOPT_HTTPHEADER);

	if (http_header != nullptr) {
		zval copy_header;
		ZVAL_DUP(&copy_header, http_header);
		add_index_zval(&OPENTELEMETRY_G(curl_header), cid, &copy_header);
	}

	opentelemetry_original_curl_setopt_array(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

void opentelemetry_curl_close_handler(INTERNAL_FUNCTION_PARAMETERS) {

	if (!is_has_provider()) {
		opentelemetry_original_curl_close(INTERNAL_FUNCTION_PARAM_PASSTHRU);
		return;
	}

	zval *zid;

#if PHP_VERSION_ID >= 80000
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(zid, curl_ce)
	ZEND_PARSE_PARAMETERS_END();
	zend_ulong cid = Z_OBJ_HANDLE_P(zid);
#else
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &zid) == FAILURE) {
		return;
	}
	zend_ulong cid = Z_RES_HANDLE_P(zid);
#endif

	zval *http_header = zend_hash_index_find(Z_ARRVAL_P(&OPENTELEMETRY_G(curl_header)), cid);
	if (http_header != nullptr) {
		zend_hash_index_del(Z_ARRVAL_P(&OPENTELEMETRY_G(curl_header)), cid);
	}

	opentelemetry_original_curl_close(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

void register_zend_hook_curl() {
	// bind curl
	zend_function *old_function;
	if ((old_function = OPENTELEMETRY_OLD_FN("curl_exec")) != nullptr) {
		opentelemetry_original_curl_exec = old_function->internal_function.handler;
		old_function->internal_function.handler = opentelemetry_curl_exec_handler;
	}

	if ((old_function = OPENTELEMETRY_OLD_FN("curl_setopt")) != nullptr) {
		opentelemetry_original_curl_setopt = old_function->internal_function.handler;
		old_function->internal_function.handler = opentelemetry_curl_setopt_handler;
	}

	if ((old_function = OPENTELEMETRY_OLD_FN("curl_setopt_array")) != nullptr) {
		opentelemetry_original_curl_setopt_array = old_function->internal_function.handler;
		old_function->internal_function.handler = opentelemetry_curl_setopt_array_handler;
	}

	if ((old_function = OPENTELEMETRY_OLD_FN("curl_close")) != nullptr) {
		opentelemetry_original_curl_close = old_function->internal_function.handler;
		old_function->internal_function.handler = opentelemetry_curl_close_handler;
	}

	error_maps = {
		{1, "CURLE_UNSUPPORTED_PROTOCOL"},
		{2, "CURLE_FAILED_INIT"},
		{3, "CURLE_URL_MALFORMAT"},
		{4, "CURLE_URL_MALFORMAT_USER"},
		{5, "CURLE_COULDNT_RESOLVE_PROXY"},
		{6, "CURLE_COULDNT_RESOLVE_HOST"},
		{7, "CURLE_COULDNT_CONNECT"},
		{8, "CURLE_FTP_WEIRD_SERVER_REPLY"},
		{9, "CURLE_REMOTE_ACCESS_DENIED"},
		{11, "CURLE_FTP_WEIRD_PASS_REPLY"},
		{13, "CURLE_FTP_WEIRD_PASV_REPLY"},
		{14, "CURLE_FTP_WEIRD_227_FORMAT"},
		{15, "CURLE_FTP_CANT_GET_HOST"},
		{17, "CURLE_FTP_COULDNT_SET_TYPE"},
		{18, "CURLE_PARTIAL_FILE"},
		{19, "CURLE_FTP_COULDNT_RETR_FILE"},
		{21, "CURLE_QUOTE_ERROR"},
		{22, "CURLE_HTTP_RETURNED_ERROR"},
		{23, "CURLE_WRITE_ERROR"},
		{25, "CURLE_UPLOAD_FAILED"},
		{26, "CURLE_READ_ERROR"},
		{27, "CURLE_OUT_OF_MEMORY"},
		{28, "CURLE_OPERATION_TIMEDOUT"},
		{30, "CURLE_FTP_PORT_FAILED"},
		{31, "CURLE_FTP_COULDNT_USE_REST"},
		{33, "CURLE_RANGE_ERROR"},
		{34, "CURLE_HTTP_POST_ERROR"},
		{35, "CURLE_SSL_CONNECT_ERROR"},
		{36, "CURLE_BAD_DOWNLOAD_RESUME"},
		{37, "CURLE_FILE_COULDNT_READ_FILE"},
		{38, "CURLE_LDAP_CANNOT_BIND"},
		{39, "CURLE_LDAP_SEARCH_FAILED"},
		{41, "CURLE_FUNCTION_NOT_FOUND"},
		{42, "CURLE_ABORTED_BY_CALLBACK"},
		{43, "CURLE_BAD_FUNCTION_ARGUMENT"},
		{45, "CURLE_INTERFACE_FAILED"},
		{47, "CURLE_TOO_MANY_REDIRECTS"},
		{48, "CURLE_UNKNOWN_TELNET_OPTION"},
		{49, "CURLE_TELNET_OPTION_SYNTAX"},
		{51, "CURLE_PEER_FAILED_VERIFICATION"},
		{52, "CURLE_GOT_NOTHING"},
		{53, "CURLE_SSL_ENGINE_NOTFOUND"},
		{54, "CURLE_SSL_ENGINE_SETFAILED"},
		{55, "CURLE_SEND_ERROR"},
		{56, "CURLE_RECV_ERROR"},
		{58, "CURLE_SSL_CERTPROBLEM"},
		{59, "CURLE_SSL_CIPHER"},
		{60, "CURLE_SSL_CACERT"},
		{61, "CURLE_BAD_CONTENT_ENCODING"},
		{62, "CURLE_LDAP_INVALID_URL"},
		{63, "CURLE_FILESIZE_EXCEEDED"},
		{64, "CURLE_USE_SSL_FAILED"},
		{65, "CURLE_SEND_FAIL_REWIND"},
		{66, "CURLE_SSL_ENGINE_INITFAILED"},
		{67, "CURLE_LOGIN_DENIED"},
		{68, "CURLE_TFTP_NOTFOUND"},
		{69, "CURLE_TFTP_PERM"},
		{70, "CURLE_REMOTE_DISK_FULL"},
		{71, "CURLE_TFTP_ILLEGAL"},
		{72, "CURLE_TFTP_UNKNOWNID"},
		{73, "CURLE_REMOTE_FILE_EXISTS"},
		{74, "CURLE_TFTP_NOSUCHUSER"},
		{75, "CURLE_CONV_FAILED"},
		{76, "CURLE_CONV_REQD"},
		{77, "CURLE_SSL_CACERT_BADFILE"},
		{78, "CURLE_REMOTE_FILE_NOT_FOUND"},
		{79, "CURLE_SSH"},
		{80, "CURLE_SSL_SHUTDOWN_FAILED"},
		{81, "CURLE_AGAIN"},
		{82, "CURLE_SSL_CRL_BADFILE"},
		{83, "CURLE_SSL_ISSUER_ERROR"},
		{84, "CURLE_FTP_PRET_FAILED"},
		{84, "CURLE_FTP_PRET_FAILED"},
		{85, "CURLE_RTSP_CSEQ_ERROR"},
		{86, "CURLE_RTSP_SESSION_ERROR"},
		{87, "CURLE_FTP_BAD_FILE_LIST"},
		{88, "CURLE_CHUNK_FAILED"},
	};
}

void unregister_zend_hook_curl() {

	zend_function *old_function;
	if ((old_function = OPENTELEMETRY_OLD_FN("curl_exec")) != nullptr) {
		old_function->internal_function.handler = opentelemetry_original_curl_exec;
	}

	if ((old_function = OPENTELEMETRY_OLD_FN("curl_setopt")) != nullptr) {
		old_function->internal_function.handler = opentelemetry_original_curl_setopt;
	}

	if ((old_function = OPENTELEMETRY_OLD_FN("curl_setopt_array")) != nullptr) {
		old_function->internal_function.handler = opentelemetry_original_curl_setopt_array;
	}

	if ((old_function = OPENTELEMETRY_OLD_FN("curl_close")) != nullptr) {
		old_function->internal_function.handler = opentelemetry_original_curl_close;
	}

	error_maps.clear();
}
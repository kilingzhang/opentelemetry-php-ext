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
    std::string tracestate = Provider::formatTraceStateHeader();
    add_next_index_string(option, ("tracestate: " + tracestate).c_str());

//    set_string_attribute(span->add_attributes(), "http.header", opentelemetry_json_encode(option));

    traceparent.shrink_to_fit();
    tracestate.shrink_to_fit();

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
      Provider::errorEnd(span, Z_STRVAL(curl_error));

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

}
//
// Created by kilingzhang on 2021/6/16.
//

#include "php_opentelemetry.h"
#include "include/otel_exporter.h"
#include "include/core.h"
#include "include/zend_hook.h"
#include "include/utils.h"
#include <string>

#include <grpc/support/log.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/create_channel.h>
#include <thread>

#include <boost/interprocess/ipc/message_queue.hpp>
#include <iostream>
#include <vector>

using namespace boost::interprocess;
using namespace opentelemetry::proto::trace::v1;
using opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest;

void exporterOpentelemetry() {

  auto request = OPENTELEMETRY_G(provider)->getRequest();
  std::string msg = request->SerializePartialAsString();
  try {

    //Create a message_queue.
    message_queue mq(
        boost::interprocess::open_only,
        "opentelemetry"
    );

    auto rtn = mq.try_send(msg.data(), msg.size(), 0);
    log("send : " + std::to_string(rtn));
  } catch (interprocess_exception &ex) {
    log("flush message_queue ex : " + std::string(ex.what()));
  }

}

[[noreturn]] void consumer() {

  while (true) {

    try {
      //Open a message queue.
      message_queue mq
          (
              open_only,//only create
              "opentelemetry"//name
          );

      std::string data;
      data.resize(100000);
      size_t msg_size;
      unsigned msg_priority;
      mq.receive(&data[0], data.size(), msg_size, msg_priority);
      if (!data.empty()) {

        data.resize(msg_size);
        auto request = new ExportTraceServiceRequest();
        request->ParseFromString(data);

        log("consumer  request trace id : " + to_hex(string2char(request->resource_spans().Get(0).instrumentation_library_spans().Get(0).spans().Get(0).trace_id()), 16) + " ByteSizeLong : " + std::to_string(request->ByteSizeLong()) + " pid:" + std::to_string(getpid()));

        grpc::ChannelArguments args;
        args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
        args.SetInt(GRPC_ARG_MIN_RECONNECT_BACKOFF_MS, 1000);
        args.SetInt(GRPC_ARG_INITIAL_RECONNECT_BACKOFF_MS, 1000);
        auto otelExporter = new OtelExporter(grpc::CreateCustomChannel(
            OPENTELEMETRY_G(grpc), grpc::InsecureChannelCredentials(), args));
        if (otelExporter) {
          otelExporter->sendTracer(request, OPENTELEMETRY_G(grpc_timeout_milliseconds));
        } else {
          log("otelExporter none");
        }

      } else {
        log("receive none");
      }

    } catch (interprocess_exception &ex) {
      log("flush message_queue ex : " + std::string(ex.what()));
    }
  }
}

void opentelemetry_module_init() {
  if (!is_enabled()) {
    return;
  }

  //初始化日志目录
  if (access(OPENTELEMETRY_G(log_path), 0) == -1) {
    php_stream_mkdir(OPENTELEMETRY_G(log_path), 0777, PHP_STREAM_MKDIR_RECURSIVE, nullptr);
  }

  OPENTELEMETRY_G(ipv4) = get_current_machine_ip(DEFAULT_ETH_INF);

  try {
    //Erase previous message queue
    message_queue(
        boost::interprocess::open_or_create,
        "opentelemetry",
        1024,
        100000,
        boost::interprocess::permissions(0666)
    );
  } catch (interprocess_exception &ex) {
    log("flush message_queue ex : " + std::string(ex.what()));
  }

  std::thread th(consumer);
  th.detach();

  register_zend_hook();
}

void opentelemetry_module_shutdown() {
  if (!is_enabled()) {
    return;
  }
  unregister_zend_hook();
  OPENTELEMETRY_G(ipv4).shrink_to_fit();
}

void opentelemetry_request_init() {
  if (!is_enabled()) {
    return;
  }
  start_tracer("", "", Span_SpanKind::Span_SpanKind_SPAN_KIND_SERVER);
}

void opentelemetry_request_shutdown() {
  if (!is_enabled()) {
    return;
  }
  shutdown_tracer();
}

void start_tracer(std::string traceparent, std::string tracestate, Span_SpanKind kind) {
  if ((is_cli_sapi() && !is_cli_enabled()) || (is_cli_sapi() && is_cli_enabled() && !is_started_cli_tracer())) {
    return;
  }

  if (!is_has_provider()) {
    OPENTELEMETRY_G(provider) = new Provider();
  }

  array_init(&OPENTELEMETRY_G(curl_header));

  if (!is_cli_sapi() && is_equal_const(OPENTELEMETRY_G(service_name), PHP_OPENTELEMETRY_SERVICE_NAME)) {
    //默认项目名为server_name
    std::string service_name =
        find_server_string(OPENTELEMETRY_G(service_name_key), strlen(OPENTELEMETRY_G(service_name_key)));
    if (!service_name.empty()) {
      OPENTELEMETRY_G(service_name) = string2char(service_name);
    }
    service_name.shrink_to_fit();
  }

  std::string uri;
  if (is_cli_sapi()) {

    zval copy_value;
    uri = find_server_string("SCRIPT_NAME", sizeof("SCRIPT_NAME") - 1);
    zval *argv = find_server_zval("argv", sizeof("argv") - 1);
    ZVAL_DUP(&copy_value, argv);
    auto span = OPENTELEMETRY_G(provider)->createFirstSpan(uri, kind);
    OPENTELEMETRY_G(provider)->parseTraceParent(traceparent);
    OPENTELEMETRY_G(provider)->parseTraceState(tracestate);
    set_string_attribute(span->add_attributes(), COMPONENTS_KEY, COMPONENTS_CLI);
    set_string_attribute(span->add_attributes(), "cli.argv", opentelemetry_json_encode(&copy_value));
    zval_dtor(&copy_value);

  } else {

    uri = find_server_string("SCRIPT_URL", sizeof("SCRIPT_URL") - 1);
    traceparent = find_server_string("HTTP_TRACEPARENT", sizeof("HTTP_TRACEPARENT") - 1);
    tracestate = find_server_string("HTTP_TRACESTATE", sizeof("HTTP_TRACESTATE") - 1);
    auto span = OPENTELEMETRY_G(provider)->createFirstSpan(uri, kind);
    OPENTELEMETRY_G(provider)->parseTraceParent(traceparent);
    OPENTELEMETRY_G(provider)->parseTraceState(tracestate);
    std::string request_method = find_server_string("REQUEST_METHOD", sizeof("REQUEST_METHOD") - 1);
    std::string request_query = find_server_string("QUERY_STRING", sizeof("QUERY_STRING") - 1);
    std::string request_http_host = find_server_string("HTTP_HOST", sizeof("HTTP_HOST") - 1);

    set_string_attribute(span->add_attributes(), COMPONENTS_KEY, COMPONENTS_REQUEST);
    set_string_attribute(span->add_attributes(), "request.method", request_method);
    set_string_attribute(span->add_attributes(), "request.query", request_query);
    set_string_attribute(span->add_attributes(), "request.http_host", request_http_host);
    set_string_attribute(span->add_attributes(), "request.host", request_http_host);

    request_method.shrink_to_fit();
    request_query.shrink_to_fit();
    request_http_host.shrink_to_fit();
    request_method.clear();
    request_query.clear();
    request_http_host.clear();
  }
  uri.shrink_to_fit();
  traceparent.shrink_to_fit();
  tracestate.shrink_to_fit();

}

void shutdown_tracer() {
  if (is_cli_sapi() && (!is_cli_enabled() || !is_started_cli_tracer())) {
    OPENTELEMETRY_G(is_started_cli_tracer) = false;
    return;
  }

  if (OPENTELEMETRY_G(provider)->firstOneSpan()) {
    okEnd(OPENTELEMETRY_G(provider)->firstOneSpan());
    exporterOpentelemetry();
    OPENTELEMETRY_G(provider)->clean();
  }

  OPENTELEMETRY_G(is_started_cli_tracer) = false;
  zval_dtor(&OPENTELEMETRY_G(curl_header));
}
//
// Created by kilingzhang on 2021/6/16.
//

#include "php_opentelemetry.h"
#include "include/otel_exporter.h"
#include "include/core.h"
#include "include/zend_hook.h"
#include "include/utils.h"

#include <thread>
#include <string>
#include <cstdlib>

#include <grpcpp/security/credentials.h>
#include <grpcpp/create_channel.h>

#include <boost/interprocess/ipc/message_queue.hpp>

using namespace boost::interprocess;

void exporterOpenTelemetry() {
    auto request = OPENTELEMETRY_G(provider)->getRequest();
    std::string msg = request->SerializePartialAsString();
    try {
        message_queue mq(open_only, OPENTELEMETRY_G(message_queue_name));
        if (!mq.try_send(msg.data(), msg.size(), 0)) {
            log("send message_queue failed");
        }
        msg.shrink_to_fit();
    } catch (interprocess_exception &ex) {
        log("send flush message_queue failed : " + std::string(ex.what()));
    }
}

[[noreturn]] void consumer(int i, const std::string &name, OtelExporter *exporter) {
    try {
        // Open a message queue.
        auto *mq = new message_queue(open_only,  // only create
                                     name.c_str());

        while (true) {
            std::string data;
            data.resize(OPENTELEMETRY_G(max_message_size));
            size_t msg_size;
            unsigned msg_priority;
            mq->receive(&data[0], data.size(), msg_size, msg_priority);
            if (!data.empty()) {
                data.resize(msg_size);
                if (exporter->isReceiverGRPC()) {
                    auto request = new opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest();
                    request->ParseFromString(data);
                    exporter->sendAsyncTracer(request, OPENTELEMETRY_G(grpc_timeout_milliseconds));
                    request->Clear();
                    delete request;
                }
                if (exporter->isReceiverUDP()) {
                    exporter->sendTracerByUDP(data);
                }
            }
            data.shrink_to_fit();
        }

    } catch (interprocess_exception &ex) {
        log("i : " + std::to_string(i) + " name : " + name + " consumer failed : " + std::string(ex.what()));
    }
}

void init_consumers() {
    if (OPENTELEMETRY_G(is_init_consumers)) {
        return;
    }

    if (OPENTELEMETRY_G(message_queue_name) == nullptr || is_equal_const(OPENTELEMETRY_G(message_queue_name), "")) {
        OPENTELEMETRY_G(message_queue_name) = string2char("opentelemetry_" + std::to_string(getpid()));
    }

    // 先清理历史异常数据
    clean_consumers();
    // 标记已经初始化过消费队列
    OPENTELEMETRY_G(is_init_consumers) = true;

    // cli模式下
    if (is_cli_sapi()) {
        OPENTELEMETRY_G(consumer_nums) = OPENTELEMETRY_G(cli_consumer_nums);
    }

    try {
        // Erase previous message queue
        message_queue(open_or_create,
                      OPENTELEMETRY_G(message_queue_name),
                      OPENTELEMETRY_G(max_queue_length),
                      OPENTELEMETRY_G(max_message_size),
                      permissions(0777));

        log("open " + std::string(OPENTELEMETRY_G(message_queue_name)) + " message queue success .");

        for (int i = 0; i < OPENTELEMETRY_G(consumer_nums); i++) {
            auto otelExporter = new OtelExporter(OPENTELEMETRY_G(receiver_type));

            if (is_receiver_grpc()) {
                grpc::ChannelArguments args;
                args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 100);
                args.SetInt(GRPC_ARG_MIN_RECONNECT_BACKOFF_MS, 100);
                args.SetInt(GRPC_ARG_INITIAL_RECONNECT_BACKOFF_MS, 100);
                std::shared_ptr<grpc::ChannelInterface> channel =
                    grpc::CreateCustomChannel(OPENTELEMETRY_G(grpc_endpoint), grpc::InsecureChannelCredentials(), args);
                otelExporter->initGRPC(channel);
                // 启动新线程，从队列中取出结果并处理
                std::thread asyncCompleteRpc(&OtelExporter::AsyncCompleteRpc, otelExporter);
                asyncCompleteRpc.detach();
            }

            if (is_receiver_udp()) {
                otelExporter->initUDP(OPENTELEMETRY_G(udp_ip), OPENTELEMETRY_G(udp_port));
            }

            std::thread con(consumer, i, OPENTELEMETRY_G(message_queue_name), otelExporter);
            con.detach();
        }
    } catch (interprocess_exception &ex) {
        log("open flush message_queue ex : " + std::string(ex.what()));
    }
}

void clean_consumers() {
    try {
        message_queue::remove(OPENTELEMETRY_G(message_queue_name));
        log("remove " + std::string(OPENTELEMETRY_G(message_queue_name)) + " message queue success .");
    } catch (interprocess_exception &ex) {
        log("remove flush message_queue ex : " + std::string(ex.what()));
    }
}

void opentelemetry_module_init() {
    // 未开启扩展 或者 cli模式下未开启cli收集
    if (!is_enabled() || (is_cli_sapi() && !is_cli_enabled())) {
        return;
    }

    // 初始化日志目录
    if (access(OPENTELEMETRY_G(log_path), 0) == -1) {
        php_stream_mkdir(OPENTELEMETRY_G(log_path), 0777, PHP_STREAM_MKDIR_RECURSIVE, nullptr);
    }

    OPENTELEMETRY_G(message_queue_name) = string2char("opentelemetry_" + std::to_string(getpid()));

    debug("message_queue_name: " + std::string(OPENTELEMETRY_G(message_queue_name)));

    gethostname(OPENTELEMETRY_G(hostname), sizeof(OPENTELEMETRY_G(hostname)));

    debug("hostname: " + std::string(OPENTELEMETRY_G(hostname)));

    OPENTELEMETRY_G(ipv4) = get_current_machine_ip(DEFAULT_ETH_INF);

    debug("ipv4: " + std::string(OPENTELEMETRY_G(ipv4)));

    // cli 模式下  默认不初始化消费队列 消费队列在主动开启收集时初始化
    if (!is_cli_sapi()) {
        init_consumers();
    }

    // 注册hook
    register_zend_hook();

    // debug 专用
    //    // 用户自定义函数执行器(php脚本定义的类、函数)
    //    opentelemetry_original_zend_execute_ex = zend_execute_ex;
    //    zend_execute_ex = opentelemetry_trace_execute_ex;
    //
    //    // 内部函数执行器(c语言定义的类、函数)
    //    opentelemetry_original_zend_execute_internal = zend_execute_internal;
    //    zend_execute_internal = opentelemetry_trace_execute_internal;
}

void opentelemetry_module_shutdown() {
    // 未开启扩展 或者 cli模式下未开启cli收集
    if (!is_enabled() || (is_cli_sapi() && !is_cli_enabled())) {
        return;
    }

    // 开启扩展 cli模式下 开启了cli收集 并且初始化了消费队列
    if (is_enabled() && is_cli_sapi() && is_cli_enabled() && OPENTELEMETRY_G(is_init_consumers)) {
        // 退出进程前 清理消费队列
        clean_consumers();
    }
    // 注销hook
    unregister_zend_hook();
    OPENTELEMETRY_G(ipv4).shrink_to_fit();
}

void opentelemetry_request_init() {
    if (!is_enabled() || is_cli_sapi()) {
        return;
    }
    debug("request_init");
    start_tracer("", "", opentelemetry::proto::trace::v1::Span_SpanKind::Span_SpanKind_SPAN_KIND_SERVER);
}

void opentelemetry_request_shutdown() {
    if (!is_enabled() || is_cli_sapi()) {
        return;
    }
    debug("request_shutdown");
    shutdown_tracer();
}

void start_tracer(std::string traceparent,
                  std::string tracebaggage,
                  opentelemetry::proto::trace::v1::Span_SpanKind kind) {
    if ((is_cli_sapi() && !is_cli_enabled()) || (is_cli_sapi() && is_cli_enabled() && !is_started_cli_tracer())) {
        return;
    }

    debug("start_tracer");
    // cli 模式下 或者 判断 未初始化 Provider 初始化 Provider 初始化消费队列
    init_consumers();
    if (!is_has_provider()) {
        OPENTELEMETRY_G(provider) = new Provider();
    }

    if (is_has_provider()) {
        OPENTELEMETRY_G(provider)->clean();
        OPENTELEMETRY_G(provider)->increase();
    }

    array_init(&OPENTELEMETRY_G(curl_header));

    if (!is_cli_sapi() && is_equal_const(OPENTELEMETRY_G(service_name), PHP_OPENTELEMETRY_SERVICE_NAME)) {
        // 默认项目名为server_name
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
        if (kind == Span_SpanKind_SPAN_KIND_UNSPECIFIED) {
            kind = Span_SpanKind_SPAN_KIND_INTERNAL;
        }
        auto span = OPENTELEMETRY_G(provider)->createFirstSpan(uri, kind);
        OPENTELEMETRY_G(provider)->parseTraceParent(traceparent);
        OPENTELEMETRY_G(provider)->parseBaggage(tracebaggage);
        set_string_attribute(span->add_attributes(), "process.executable.name", uri);
        set_string_attribute(span->add_attributes(), "process.command_args", opentelemetry_json_encode(&copy_value));
        set_string_attribute(span->add_attributes(), "process.pid", std::to_string(getpid()));
        zval_dtor(&copy_value);

    } else {
        std::string request_uri = find_server_string("REQUEST_URI", sizeof("REQUEST_URI") - 1);
        size_t pos = request_uri.find('?');
        if (pos != std::string::npos) {
            uri = request_uri.substr(0, pos);
        } else {
            uri = request_uri;
        }
        traceparent = find_server_string("HTTP_TRACEPARENT", sizeof("HTTP_TRACEPARENT") - 1);
        tracebaggage = find_server_string("HTTP_BAGGAGE", sizeof("HTTP_BAGGAGE") - 1);
        auto span = OPENTELEMETRY_G(provider)->createFirstSpan(uri, kind);
        OPENTELEMETRY_G(provider)->parseTraceParent(traceparent);
        OPENTELEMETRY_G(provider)->parseBaggage(tracebaggage);
        std::string request_method = find_server_string("REQUEST_METHOD", sizeof("REQUEST_METHOD") - 1);
        std::string request_query = find_server_string("QUERY_STRING", sizeof("QUERY_STRING") - 1);
        std::string request_http_host = find_server_string("HTTP_HOST", sizeof("HTTP_HOST") - 1);
        std::string request_http_port = find_server_string("SERVER_PORT", sizeof("SERVER_PORT") - 1);
        std::string request_user_agent = find_server_string("HTTP_USER_AGENT", sizeof("HTTP_USER_AGENT") - 1);
        std::string request_remote_ip = find_server_string("REMOTE_ADDR", sizeof("REMOTE_ADDR") - 1);

        set_string_attribute(span->add_attributes(), "http.method", request_method);

        // 记录POST传参数
        if (is_equal(request_method, "POST")) {
            zval *post = get_post_zval();
            if (post) {
                set_string_attribute(span->add_attributes(), "http.params", opentelemetry_json_encode(post));
            }
        }

        set_string_attribute(span->add_attributes(), "http.host", request_http_host);
        set_string_attribute(span->add_attributes(), "http.target", request_uri);
        set_string_attribute(span->add_attributes(), "http.client_ip", request_remote_ip);
        set_string_attribute(span->add_attributes(), "http.user_agent", request_user_agent);
        set_string_attribute(span->add_attributes(), "net.host.ip", OPENTELEMETRY_G(ipv4));
        set_int64_attribute(span->add_attributes(), "net.host.port", strtol(request_http_port.c_str(), nullptr, 10));

        request_method.shrink_to_fit();
        request_query.shrink_to_fit();
        request_http_host.shrink_to_fit();
        request_method.clear();
        request_query.clear();
        request_http_host.clear();
    }
    uri.shrink_to_fit();
    traceparent.shrink_to_fit();
    tracebaggage.shrink_to_fit();
}

void shutdown_tracer() {
    if (is_cli_sapi() && (!is_cli_enabled() || !is_started_cli_tracer())) {
        OPENTELEMETRY_G(is_started_cli_tracer) = false;
        return;
    }

    debug("shutdown_tracer");
    if (OPENTELEMETRY_G(provider)->firstOneSpan()) {
        Provider::okEnd(OPENTELEMETRY_G(provider)->firstOneSpan());
        OPENTELEMETRY_G(provider)->getTracer()->set_allocated_resource(OPENTELEMETRY_G(provider)->getResource());
        if (OPENTELEMETRY_G(enable_collect) && OPENTELEMETRY_G(provider)->isSampled()) {
            //      unsigned long s = get_unix_nanoseconds();
            exporterOpenTelemetry();
            // log("exporterOpenTelemetry cost : " + std::to_string(double(get_unix_nanoseconds() - s) /
            // 1000000.0) + "ms.");
        }
        OPENTELEMETRY_G(provider)->clean();
        if (is_cli_sapi()) {
            delete OPENTELEMETRY_G(provider);
            OPENTELEMETRY_G(provider) = nullptr;
        }
    }

    OPENTELEMETRY_G(is_started_cli_tracer) = false;
    zval_dtor(&OPENTELEMETRY_G(curl_header));
}

/**
 * This method replaces the internal zend_execute_ex method used to dispatch
 * calls to user space code. The original zend_execute_ex method is moved to
 * opentelemetry_original_zend_execute_ex
 */
void opentelemetry_trace_execute_ex(zend_execute_data *execute_data TSRMLS_DC) {
    opentelemetry_trace_execute(0, execute_data TSRMLS_CC, NULL);
}

/**
 * This method resumes the internal function execution.
 */
static void resume_execute_internal(INTERNAL_FUNCTION_PARAMETERS) {
    if (opentelemetry_original_zend_execute_internal) {
        opentelemetry_original_zend_execute_internal(INTERNAL_FUNCTION_PARAM_PASSTHRU);
    } else {
        execute_data->func->internal_function.handler(INTERNAL_FUNCTION_PARAM_PASSTHRU);
    }
}

void opentelemetry_trace_execute(zend_bool is_internal, INTERNAL_FUNCTION_PARAMETERS) {
    std::string function_name;
    zend_execute_data *caller = EG(current_execute_data);

    if (caller->func->common.function_name) {
        function_name = find_trace_add_scope_name(
            caller->func->common.function_name, caller->func->common.scope, caller->func->common.fn_flags);
    } else if (caller->func->internal_function.function_name) {
        function_name = find_trace_add_scope_name(caller->func->internal_function.function_name,
                                                  caller->func->internal_function.scope,
                                                  caller->func->internal_function.fn_flags);
    }

    log(function_name);
    if (is_internal) {
        resume_execute_internal(INTERNAL_FUNCTION_PARAM_PASSTHRU);
    } else {
        opentelemetry_original_zend_execute_ex(execute_data TSRMLS_CC);
    }
}

void opentelemetry_trace_execute_internal(INTERNAL_FUNCTION_PARAMETERS) {
    opentelemetry_trace_execute(1, INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

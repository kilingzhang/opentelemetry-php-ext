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

void exporterOpentelemetry() {

	auto request = OPENTELEMETRY_G(provider)->getRequest();
	std::string msg = request->SerializePartialAsString();

	try {
		message_queue mq(
			open_only,
			OPENTELEMETRY_G(message_queue_name)
		);

		int msg_length = static_cast<int>(msg.size());
		int max_length = OPENTELEMETRY_G(grpc_max_message_size);
		if (msg_length > max_length) {
			log("[opentelemetry] " + OPENTELEMETRY_G(provider)->firstOneSpan()->name() + " message is too big: " + std::to_string(msg_length) + ", mq_max_message_length=" + std::to_string(max_length));
			return;
		}

		if (!mq.try_send(msg.data(), msg.size(), 0)) {
			log("[opentelemetry] send message_queue failed");
		}

		msg.shrink_to_fit();
	} catch (interprocess_exception &ex) {
		log("[opentelemetry] send flush message_queue failed : " + std::string(ex.what()));
	}

}

[[noreturn]] void consumer(int i, const std::string &name, OtelExporter *otelExporter) {

	try {
		//Open a message queue.
		auto *mq =
			new message_queue(
				open_only,//only create
				name.c_str()
			);

		while (true) {

			std::string data;
			data.resize(OPENTELEMETRY_G(grpc_max_message_size));
			size_t msg_size;
			unsigned msg_priority;
			mq->receive(&data[0], data.size(), msg_size, msg_priority);
			if (!data.empty()) {
				data.resize(msg_size);
				auto request = new opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest();
				request->ParseFromString(data);
				otelExporter->sendAsyncTracer(request, OPENTELEMETRY_G(grpc_timeout_milliseconds));
				request->Clear();
				delete request;
			}
			data.shrink_to_fit();

		}

	} catch (interprocess_exception &ex) {
		log("[opentelemetry] i : " + std::to_string(i) + " name : " + name + " consumer failed : " + std::string(ex.what()));
	}
}

void init_grpc_consumers() {

	clean_grpc_consumers();

	try {
		//Erase previous message queue
		message_queue(
			open_or_create,
			OPENTELEMETRY_G(message_queue_name),
			OPENTELEMETRY_G(grpc_max_queue_length),
			OPENTELEMETRY_G(grpc_max_message_size),
			permissions(0777)
		);

		log("open " + std::string(OPENTELEMETRY_G(message_queue_name)) + " message queue success .");

		for (int i = 0; i < OPENTELEMETRY_G(grpc_consumer); i++) {

			grpc::ChannelArguments args;
			args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 100);
			args.SetInt(GRPC_ARG_MIN_RECONNECT_BACKOFF_MS, 100);
			args.SetInt(GRPC_ARG_INITIAL_RECONNECT_BACKOFF_MS, 100);
			auto otelExporter = new OtelExporter(grpc::CreateCustomChannel(
				OPENTELEMETRY_G(grpc_endpoint), grpc::InsecureChannelCredentials(), args));

			// 启动新线程，从队列中取出结果并处理
			std::thread asyncCompleteRpc(&OtelExporter::AsyncCompleteRpc, otelExporter);
			asyncCompleteRpc.detach();

			std::thread con(consumer, i, OPENTELEMETRY_G(message_queue_name), otelExporter);
			con.detach();

		}
	} catch (interprocess_exception &ex) {
		log("open flush message_queue ex : " + std::string(ex.what()));
	}

}

void clean_grpc_consumers() {
	try {
		message_queue::remove(OPENTELEMETRY_G(message_queue_name));
		log("remove " + std::string(OPENTELEMETRY_G(message_queue_name)) + " message queue success .");
	} catch (interprocess_exception &ex) {
		log("remove flush message_queue ex : " + std::string(ex.what()));
	}
}

void opentelemetry_module_init() {

	if (!is_enabled() || (is_cli_sapi() && !is_cli_enabled())) {
		return;
	}

	OPENTELEMETRY_G(message_queue_name) = string2char("opentelemetry_" + std::to_string(getpid()));

	gethostname(OPENTELEMETRY_G(hostname), sizeof(OPENTELEMETRY_G(hostname)));
	//初始化日志目录
	if (access(OPENTELEMETRY_G(log_path), 0) == -1) {
		php_stream_mkdir(OPENTELEMETRY_G(log_path), 0777, PHP_STREAM_MKDIR_RECURSIVE, nullptr);
	}

	OPENTELEMETRY_G(ipv4) = get_current_machine_ip(DEFAULT_ETH_INF);

	if (!is_cli_sapi()) {
		init_grpc_consumers();
	}

	register_zend_hook();
}

void opentelemetry_module_shutdown() {

	if (!is_enabled() || (is_cli_sapi() && !is_cli_enabled())) {
		return;
	}
	unregister_zend_hook();
	OPENTELEMETRY_G(ipv4).shrink_to_fit();
}

void opentelemetry_request_init() {
	if (!is_enabled()) {
		return;
	}
	start_tracer("", "", opentelemetry::proto::trace::v1::Span_SpanKind::Span_SpanKind_SPAN_KIND_SERVER);
}

void opentelemetry_request_shutdown() {
	if (!is_enabled()) {
		return;
	}
	shutdown_tracer();
}

void start_tracer(std::string traceparent, std::string tracestate, opentelemetry::proto::trace::v1::Span_SpanKind kind) {
	if ((is_cli_sapi() && !is_cli_enabled()) || (is_cli_sapi() && is_cli_enabled() && !is_started_cli_tracer())) {
		return;
	}

	if (!is_has_provider()) {
		OPENTELEMETRY_G(provider) = new Provider();

		if (is_cli_sapi()) {
			init_grpc_consumers();
		}

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
		if (kind == Span_SpanKind_SPAN_KIND_UNSPECIFIED) {
			kind = Span_SpanKind_SPAN_KIND_INTERNAL;
		}
		auto span = OPENTELEMETRY_G(provider)->createFirstSpan(uri, kind);
		OPENTELEMETRY_G(provider)->parseTraceParent(traceparent);
		OPENTELEMETRY_G(provider)->parseTraceState(tracestate);
		set_string_attribute(span->add_attributes(), "process.executable.name", uri);
		set_string_attribute(span->add_attributes(), "process.command_args", opentelemetry_json_encode(&copy_value));
		set_string_attribute(span->add_attributes(), "process.pid", std::to_string(getpid()));
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
		std::string request_http_port = find_server_string("SERVER_PORT", sizeof("SERVER_PORT") - 1);
		std::string request_user_agent = find_server_string("HTTP_USER_AGENT", sizeof("HTTP_USER_AGENT") - 1);
		std::string request_remote_ip = find_server_string("REMOTE_ADDR", sizeof("REMOTE_ADDR") - 1);
		std::string request_uri = find_server_string("REQUEST_URI", sizeof("REQUEST_URI") - 1);

		set_string_attribute(span->add_attributes(), "http.method", request_method);
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
	tracestate.shrink_to_fit();

}

void shutdown_tracer() {
	if (is_cli_sapi() && (!is_cli_enabled() || !is_started_cli_tracer())) {
		OPENTELEMETRY_G(is_started_cli_tracer) = false;
		return;
	}

	if (OPENTELEMETRY_G(provider)->firstOneSpan()) {
		Provider::okEnd(OPENTELEMETRY_G(provider)->firstOneSpan());
		OPENTELEMETRY_G(provider)->getTracer()->set_allocated_resource(OPENTELEMETRY_G(provider)->getResource());
		if (OPENTELEMETRY_G(enable_collect) && OPENTELEMETRY_G(provider)->isSampled()) {
//      unsigned long s = get_unix_nanoseconds();
			exporterOpentelemetry();
//      log("exporterOpentelemetry cost : " + std::to_string(double(get_unix_nanoseconds() - s) / 1000000.0) + "ms.");
		}
		OPENTELEMETRY_G(provider)->clean();
	}

	OPENTELEMETRY_G(is_started_cli_tracer) = false;
	zval_dtor(&OPENTELEMETRY_G(curl_header));
}
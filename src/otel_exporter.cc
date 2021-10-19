//
// Created by kilingzhang on 2021/7/23.
//
#include "include/otel_exporter.h"
#include "include/utils.h"
#include "include/Timer.h"
#include "include/hex.h"

#include <string>

#include <grpc/support/log.h>
#include <netdb.h>

#include "opentelemetry/proto/collector/trace/v1/trace_service.pb.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.h"

using namespace opentelemetry::proto::collector::trace::v1;

OtelExporter::OtelExporter(const char *type) {
	receiver_type = type;
}

OtelExporter::~OtelExporter() {
	if (isReceiverGRPC()) {
		cq_.Shutdown();
	}
	if (isReceiverUDP()) {
		close(sock_fd);
	}
};

void OtelExporter::initGRPC(const std::shared_ptr<grpc::ChannelInterface> &channel) {
	stub_ = TraceService::NewStub(channel);
}

void OtelExporter::initUDP(const char *ip, int port) {
	addr_ip = ip;
	addr_port = port;
	/* 建立udp socket */
	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock_fd < 0) {
		log("[opentelemetry] sock_fd open error:" + std::string(strerror(errno)));
		return;
	}
	resolveUDPAddr();
	auto *pTimer = new CTimer("dns_lookup");
	pTimer->AsyncLoop(OPENTELEMETRY_G(udp_look_up_time) * 1000, [this]() -> void { this->resolveUDPAddr(); });    //异步循环执行，间隔时间10毫秒
}

void OtelExporter::resolveUDPAddr() {
	struct addrinfo *aip;
	struct addrinfo hint{};
	char buf[INET_ADDRSTRLEN];
	const char *addr;
	int ilRc;
	hint.ai_family = AF_UNSPEC; /* hint 的限定设置 */
	hint.ai_socktype = SOCK_DGRAM; /* 这里可是设置 socket type . 比如 SOCK——DGRAM */
	hint.ai_flags = AI_PASSIVE; /* flags 的标志很多 。常用的有AI_CANONNAME; */
	hint.ai_protocol = 0; /* 设置协议 一般为0，默认 */
	hint.ai_addrlen = 0; /* 下面不可以设置，为0，或者为NULL */
	hint.ai_canonname = nullptr;
	hint.ai_addr = nullptr;
	hint.ai_next = nullptr;
	ilRc = getaddrinfo(addr_ip, std::to_string(addr_port).c_str(), &hint, &addr_info);
	if (ilRc < 0) {
		log("[opentelemetry] get_addr_info error:" + std::string(gai_strerror(errno)));
		memset(addr_in, 0, sizeof(*addr_in));
		addr_in->sin_family = AF_INET;
		addr_in->sin_addr.s_addr = inet_addr(addr_ip);
		addr_in->sin_port = htons(addr_port);
	} else {
		current_addr_info = addr_info;
		addr_in = (struct sockaddr_in *) current_addr_info->ai_addr;
	}
	log("[opentelemetry] resolveUDPAddr ip : " + std::string(inet_ntop(AF_INET, &addr_in->sin_addr, buf, INET_ADDRSTRLEN)));
}

void OtelExporter::sendTracerByUDP(const std::string &data) {
	if (sock_fd > 0 && addr_in != nullptr && !data.empty()) {

		int dataSize = static_cast<int>(data.size());
		//udp 最大报文消息
		int maxUdpSize = 60000;

		if (dataSize <= maxUdpSize) {
			ssize_t nums = sendto(sock_fd, data.c_str(), data.size(), 0, (struct sockaddr *) addr_in, sizeof(*addr_in));
			if (nums < 0) {
				char buf[INET_ADDRSTRLEN];
				log("[opentelemetry] sendto message failed, ip:" + std::string(inet_ntop(AF_INET, &addr_in->sin_addr, buf, INET_ADDRSTRLEN)) + ", size:" + std::to_string(data.size()) + ", error:" + strerror(errno));
			}
		} else {
			//超过udp最大发包大小 分包发送
			int shards = ceil((float) dataSize / (float) maxUdpSize);
			std::string id = Hex::encode(reinterpret_cast<const uint8_t *>(generate_trace_id().data), 16);
			for (int i = 0; i < shards; i++) {
				//协议 md5:9,0:end:数据
				auto warp = id + ":" + std::to_string(shards) + "," + std::to_string(i) + ":end:" + data.substr(i * maxUdpSize, maxUdpSize);
				//防止丢包 多发一次
				sendto(sock_fd, warp.c_str(), warp.size(), 0, (struct sockaddr *) addr_in, sizeof(*addr_in));
				ssize_t nums = sendto(sock_fd, warp.c_str(), warp.size(), 0, (struct sockaddr *) addr_in, sizeof(*addr_in));
				if (nums < 0) {
					char buf[INET_ADDRSTRLEN];
					log("[opentelemetry] sendto warp " + std::to_string(i) + " message failed, ip:" + std::string(inet_ntop(AF_INET, &addr_in->sin_addr, buf, INET_ADDRSTRLEN)) + ", size:" + std::to_string(data.size()) + ", error:" + strerror(errno));
				}
			}
		}

		if (current_addr_info != nullptr) {
			if (current_addr_info->ai_next != nullptr) {
				current_addr_info = current_addr_info->ai_next;
			} else {
				current_addr_info = addr_info;
			}
			addr_in = (struct sockaddr_in *) current_addr_info->ai_addr;
		}
	}
}

bool OtelExporter::isReceiverUDP() {
	return is_equal(receiver_type, "udp");
}

bool OtelExporter::isReceiverGRPC() {
	return is_equal(receiver_type, "grpc");
}

void OtelExporter::sendTracer(ExportTraceServiceRequest *request, long long int milliseconds) {

	grpc::ClientContext context;

	if (milliseconds > 0) {
		std::chrono::system_clock::time_point deadline = std::chrono::system_clock::now() +
			std::chrono::milliseconds(milliseconds);
		context.set_deadline(deadline);
	}

	ExportTraceServiceResponse response;

	grpc::Status status = stub_->Export(&context, *request, &response);

	if (!status.ok() && is_debug()) {
		log("[opentelemetry] Export() failed: " + status.error_message() + " trace id : " + traceId(request->resource_spans().Get(0).instrumentation_library_spans().Get(0).spans().Get(0)));
		return;
	}

}

void OtelExporter::sendAsyncTracer(ExportTraceServiceRequest *request, long long int milliseconds) {

	auto *call = new AsyncClientCall;
	call->context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(milliseconds));

	// stub_->PrepareAsyncExport() creates an RPC object, returning
	// an instance to store in "call" but does not actually start the RPC
	// Because we are using the asynchronous API, we need to hold on to
	// the "call" instance in order to get updates on the ongoing RPC.
	call->response_reader =
		stub_->PrepareAsyncExport(&call->context, *request, &cq_);

	call->response_reader->StartCall();

	// Request that, upon completion of the RPC, "reply" be updated with the
	// server's response; "status" with the indication of whether the operation
	// was successful. Tag the request with the memory address of the call
	// object.
	call->response_reader->Finish(&call->response, &call->status, (void *) call);
}

// Loop while listening for completed responses.
// Prints out the response from the server.
void OtelExporter::AsyncCompleteRpc() {
	void *got_tag;
	bool ok = false;

	// Block until the next result is available in the completion queue "cq".
	while (cq_.Next(&got_tag, &ok)) {

		// The tag in this example is the memory location of the call object
		auto *call = static_cast<AsyncClientCall *>(got_tag);

		// Verify that the request was completed successfully. Note that "ok"
		// corresponds solely to the request for updates introduced by Finish().
		GPR_ASSERT(ok);

		if (!call->status.ok() && is_debug()) {
			log("[opentelemetry] Export() failed: " + call->status.error_message());
		}
		// Once we're complete, deallocate the call object.
		delete call;

	}
};
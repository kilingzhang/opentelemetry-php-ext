//
// Created by kilingzhang on 2021/7/23.
//

#ifndef OPENTELEMETRY_SRC_OTEL_EXPORTER_H_
#define OPENTELEMETRY_SRC_OTEL_EXPORTER_H_

#include <string>
#include <grpc/support/log.h>
#include <grpc/grpc.h>
#include <netinet/in.h>
#include "opentelemetry/proto/collector/trace/v1/trace_service.pb.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.h"

class OtelExporter {
 private:

  std::string receiver_type;

  // Out of the passed in Channel comes the stub, stored here, our view of the
  // server's exposed services.
  std::unique_ptr<opentelemetry::proto::collector::trace::v1::TraceService::Stub> stub_;
  // The producer-consumer queue we use to communicate asynchronously with the
  // gRPC runtime.
  grpc::CompletionQueue cq_;

  // struct for keeping state and data information
  struct AsyncClientCall {
	// Container for the data we expect from the server.
	opentelemetry::proto::collector::trace::v1::ExportTraceServiceResponse response;

	// Context for the client. It could be used to convey extra information to
	// the server and/or tweak certain RPC behaviors.
	grpc::ClientContext context;

	// Storage for the status of the RPC upon completion.
	grpc::Status status;

	std::unique_ptr<grpc::ClientAsyncResponseReader<opentelemetry::proto::collector::trace::v1::ExportTraceServiceResponse>> response_reader;
  };
 public:
  int sock_fd = -1;
  struct sockaddr_in *addr_in{};
  const char *addr_ip{};
  int addr_port{};
  struct addrinfo *addr_info{};
  explicit OtelExporter(const char *type);
  ~OtelExporter();

  void initGRPC(const std::shared_ptr<grpc::ChannelInterface> &channel);

  void initUDP(const char *ip, int port);

  bool isReceiverUDP();

  bool isReceiverGRPC();

  void sendAsyncTracer(opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest *request, long long int milliseconds);

  void sendTracer(opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest *request, long long int milliseconds);

  // Loop while listening for completed responses.
  // Prints out the response from the server.
  void AsyncCompleteRpc();
  void resolveUDPAddr();
  void sendTracerByUDP(const std::string &data) const;
};

#endif //OPENTELEMETRY_SRC_OTEL_EXPORTER_H_

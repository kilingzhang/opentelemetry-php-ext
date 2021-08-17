//
// Created by kilingzhang on 2021/7/23.
//

#ifndef OPENTELEMETRY_SRC_OTEL_EXPORTER_H_
#define OPENTELEMETRY_SRC_OTEL_EXPORTER_H_

#include <string>
#include <grpc/support/log.h>
#include <grpc/grpc.h>
#include "opentelemetry/proto/collector/trace/v1/trace_service.pb.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.h"

class OtelExporter {
 private:
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
  explicit OtelExporter(const std::shared_ptr<grpc::ChannelInterface> &channel);
  ~OtelExporter();

  void sendAsyncTracer(opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest *request, long long int milliseconds);

  void sendTracer(opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest *request, long long int milliseconds);

  // Loop while listening for completed responses.
  // Prints out the response from the server.
  void AsyncCompleteRpc();

  void close();
};

#endif //OPENTELEMETRY_SRC_OTEL_EXPORTER_H_

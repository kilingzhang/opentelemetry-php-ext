//
// Created by kilingzhang on 2021/7/23.
//
#include "include/otel_exporter.h"
#include "include/utils.h"

#include <string>

#include <grpc/support/log.h>

#include "opentelemetry/proto/collector/trace/v1/trace_service.pb.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.h"

using namespace opentelemetry::proto::collector::trace::v1;

OtelExporter::OtelExporter(const std::shared_ptr<grpc::ChannelInterface> &channel)
    : stub_(TraceService::NewStub(channel)) {}

OtelExporter::~OtelExporter() {
  close();
};

void OtelExporter::sendTracer(ExportTraceServiceRequest *request, long long int milliseconds) {

  grpc::ClientContext context;

  if (milliseconds > 0) {
    std::chrono::system_clock::time_point deadline = std::chrono::system_clock::now() +
        std::chrono::milliseconds(milliseconds);
    context.set_deadline(deadline);
  }

  ExportTraceServiceResponse response;

  grpc::Status status = stub_->Export(&context, *request, &response);

  if (!status.ok()) {
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

    if (!call->status.ok()) {
      log("[opentelemetry] Export() failed: " + call->status.error_message());
    }
    // Once we're complete, deallocate the call object.
    delete call;

  }
};

void OtelExporter::close() {
  cq_.Shutdown();
};
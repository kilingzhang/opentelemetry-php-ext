//
// Created by kilingzhang on 2021/6/15.
//

#ifndef OPENTELEMETRY_TRACING_H
#define OPENTELEMETRY_TRACING_H

#include <string>
#include <iostream>
#include <opentelemetry/proto/common/v1/common.pb.h>
#include <opentelemetry/proto/trace/v1/trace.pb.h>
#include "tsl/robin_map.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service.pb.h"

using namespace opentelemetry::proto::collector::trace::v1;
using namespace opentelemetry::proto::trace::v1;

class Provider {
 public:
  Provider();

  ~Provider();

  ExportTraceServiceRequest *getRequest();

  ResourceSpans *getTracer();

  Span *createFirstSpan(const std::string &name, Span_SpanKind kind);

  Span *createSpan(const std::string &name, Span_SpanKind kind);

  Span *firstOneSpan();

  Span latestSpan();

  void clean();

  void parseTraceParent(const std::string &traceparent);
  void parseTraceState(const std::string &tracestate);
 private:
  ResourceSpans *resourceSpan = nullptr;
  tsl::robin_map<pid_t, ResourceSpans *> resourceSpans;
  ExportTraceServiceRequest *request = nullptr;
  tsl::robin_map<pid_t, ExportTraceServiceRequest *> requests;
  Span *firstSpan = nullptr;
  tsl::robin_map<pid_t, Span *> firstSpans;
};

#endif //OPENTELEMETRY_TRACING_H

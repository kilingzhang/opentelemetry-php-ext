//
// Created by kilingzhang on 2021/6/15.
//

#ifndef OPENTELEMETRY_TRACING_H
#define OPENTELEMETRY_TRACING_H

#include <string>
#include <iostream>
#include "tsl/robin_map.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/trace/v1/trace.pb.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service.pb.h"

class Provider {
 public:
  Provider();

  ~Provider();

  opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest *getRequest();

  opentelemetry::proto::trace::v1::ResourceSpans *getTracer();

  opentelemetry::proto::trace::v1::Span *createFirstSpan(const std::string &name, opentelemetry::proto::trace::v1::Span_SpanKind kind);

  opentelemetry::proto::trace::v1::Span *createSpan(const std::string &name, opentelemetry::proto::trace::v1::Span_SpanKind kind);

  opentelemetry::proto::trace::v1::Span *firstOneSpan();

  opentelemetry::proto::trace::v1::Span latestSpan();

  void clean();
  void parseTraceParent(const std::string &traceparent);
  void parseTraceState(const std::string &tracestate);
 private:
  opentelemetry::proto::trace::v1::ResourceSpans *resourceSpan = nullptr;
  tsl::robin_map<pid_t, opentelemetry::proto::trace::v1::ResourceSpans *> resourceSpans;
  opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest *request = nullptr;
  tsl::robin_map<pid_t, opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest *> requests;
  opentelemetry::proto::trace::v1::Span *firstSpan = nullptr;
  tsl::robin_map<pid_t, opentelemetry::proto::trace::v1::Span *> firstSpans;
};

#endif //OPENTELEMETRY_TRACING_H

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

  opentelemetry::proto::resource::v1::Resource *getResource();

  opentelemetry::proto::trace::v1::ResourceSpans *getTracer();

  opentelemetry::proto::trace::v1::Span *createFirstSpan(const std::string &name, opentelemetry::proto::trace::v1::Span_SpanKind kind);

  opentelemetry::proto::trace::v1::Span *createSpan(const std::string &name, opentelemetry::proto::trace::v1::Span_SpanKind kind);

  opentelemetry::proto::trace::v1::Span *firstOneSpan();

  opentelemetry::proto::trace::v1::Span latestSpan();

  std::string getRedisException();
  void setRedisException(const std::string &message);
  void clearRedisException();
  bool isRedisException();

  bool isSampled();

  void setSampled(bool isSampled);

  void clean();

  void addTraceStates(const std::string &key, const std::string &value);

  tsl::robin_map<std::string, std::string> getTraceStates();

  void parseTraceParent(const std::string &traceparent);
  void parseTraceState(const std::string &tracestate);

  std::string formatTraceParentHeader(opentelemetry::proto::trace::v1::Span *span);
  static std::string formatTraceStateHeader();

  static void okEnd(opentelemetry::proto::trace::v1::Span *span);
  static void errorEnd(opentelemetry::proto::trace::v1::Span *span, const std::string &message);

  void increase();
  long count() const;
 private:
  long counter = 0;
  opentelemetry::proto::trace::v1::ResourceSpans *resourceSpan = nullptr;
  opentelemetry::proto::resource::v1::Resource *resource = nullptr;
  tsl::robin_map<pid_t, opentelemetry::proto::trace::v1::ResourceSpans *> resourceSpans;
  tsl::robin_map<pid_t, opentelemetry::proto::resource::v1::Resource *> resources;
  opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest *request = nullptr;
  tsl::robin_map<pid_t, opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest *> requests;
  opentelemetry::proto::trace::v1::Span *firstSpan = nullptr;
  tsl::robin_map<pid_t, opentelemetry::proto::trace::v1::Span *> firstSpans;
  bool is_sampled = false;
  tsl::robin_map<pid_t, bool> sampled_map;
  tsl::robin_map<pid_t, std::string> redisExceptions;
  std::string redisException;
  tsl::robin_map<std::string, std::string> states;
  tsl::robin_map<pid_t, tsl::robin_map<std::string, std::string>> states_map;
};

#endif //OPENTELEMETRY_TRACING_H

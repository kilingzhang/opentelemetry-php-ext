//
// Created by kilingzhang on 2021/6/15.
//

#include "include/provider.h"
#include <utility>
#include <random>
#include "include/utils.h"
#include "php_opentelemetry.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service.pb.h"
#include <unistd.h>

#if (defined(unix) || defined(__unix__) || defined(__unix)) && !defined(__APPLE__)
#define PLATFORM_NAME "unix"
#elif defined(__linux__)
#define PLATFORM_NAME "linux"
#elif defined(__APPLE__) && defined(__MACH__)
#define PLATFORM_NAME "darwin"
#elif defined(__FreeBSD__)
#define PLATFORM_NAME "freebsd"
#else
#define PLATFORM_NAME ""
#endif

using namespace opentelemetry::proto::collector::trace::v1;
using namespace opentelemetry::proto::common::v1;

Provider::Provider() = default;
Provider::~Provider() = default;

ExportTraceServiceRequest *Provider::getRequest() {
  if (!is_cli_sapi()) {
    return request;
  } else {
    pid_t ppid = get_current_ppid();
    if (requests.find(ppid) != requests.end() && requests[ppid] != nullptr) {
      return requests[ppid];
    }
    return nullptr;
  }
}

ResourceSpans *Provider::getTracer() {
  char name[256] = {0};
  gethostname(name, sizeof(name));
  if (!is_cli_sapi()) {
    if (!request) {
      request = new ExportTraceServiceRequest();
      resourceSpan = request->add_resource_spans();
      auto resource = resourceSpan->resource().New();
      resource->set_dropped_attributes_count(0);
      set_string_attribute(resource->add_attributes(), "service.name", OPENTELEMETRY_G(service_name));
      set_string_attribute(resource->add_attributes(), "service.ip", OPENTELEMETRY_G(ipv4));
      set_string_attribute(resource->add_attributes(), "os.type", PLATFORM_NAME);
      set_string_attribute(resource->add_attributes(), "host.name", name);
      set_string_attribute(resource->add_attributes(), "process.pid", std::to_string(getpid()));
      set_string_attribute(resource->add_attributes(), "telemetry.sdk.language", "php");
      set_string_attribute(resource->add_attributes(), "deployment.environment", OPENTELEMETRY_G(environment));
      resourceSpan->set_allocated_resource(resource);
    }
    return resourceSpan;
  } else {
    pid_t ppid = get_current_ppid();
    if (requests.find(ppid) != requests.end()) {
      return resourceSpans[ppid];
    } else {
      requests[ppid] = new ExportTraceServiceRequest();
      resourceSpans[ppid] = requests[ppid]->add_resource_spans();
      auto resource = resourceSpans[ppid]->resource().New();
      resource->set_dropped_attributes_count(0);
      set_string_attribute(resource->add_attributes(), "service.name", OPENTELEMETRY_G(service_name));
      set_string_attribute(resource->add_attributes(), "service.ip", OPENTELEMETRY_G(ipv4));
      set_string_attribute(resource->add_attributes(), "os.type", PLATFORM_NAME);
      set_string_attribute(resource->add_attributes(), "host.name", name);
      set_string_attribute(resource->add_attributes(), "process.pid", std::to_string(getpid()));
      set_string_attribute(resource->add_attributes(), "telemetry.sdk.language", "php");
      set_string_attribute(resource->add_attributes(), "deployment.environment", OPENTELEMETRY_G(environment));
      resourceSpans[ppid]->set_allocated_resource(resource);
      return resourceSpans[ppid];
    }
  }
}

Span *Provider::createFirstSpan(const std::string &name, Span_SpanKind kind) {
  auto span = createSpan(name, kind);
  if (!is_cli_sapi()) {
    firstSpan = span;
  } else {
    pid_t ppid = get_current_ppid();
    firstSpans[ppid] = span;
  }
  return span;
}

Span *Provider::createSpan(const std::string &name, Span_SpanKind kind) {
  auto resource_span = getTracer()->add_instrumentation_library_spans();
  auto span = resource_span->add_spans();
  if (firstOneSpan()) {
    span->set_trace_id(firstOneSpan()->trace_id());
  } else {
    span->set_trace_id(generate_trace_id().data, 16);
  }
  span->set_name(name);
  span->set_kind(kind);
  span->set_span_id(generate_span_id().data, 8);
  span->set_start_time_unix_nano(get_unix_nanoseconds());
  span->set_dropped_attributes_count(0);
  return span;
}

Span *Provider::firstOneSpan() {
  if (!is_cli_sapi()) {
    return firstSpan;
  }
  pid_t ppid = get_current_ppid();
  if (firstSpans.find(ppid) != firstSpans.end()) {
    return firstSpans[ppid];
  }
  return nullptr;
}

Span Provider::latestSpan() {
  return *firstOneSpan();
}

void Provider::clean() {
  if (!is_cli_sapi()) {
    if (request) {
      request = nullptr;
      resourceSpan = nullptr;
      firstSpan = nullptr;
    }
  } else {
    pid_t ppid = get_current_ppid();
    if (requests.find(ppid) != requests.end()) {
      firstSpans.erase(ppid);
      resourceSpans.erase(ppid);
      requests.erase(ppid);
    }
  }
}

std::vector<std::string> explode(const std::string &delimiter, const std::string &str) {
  std::vector<std::string> arr;
  unsigned long length = str.length();
  unsigned long delLength = delimiter.length();
  if (delLength == 0) {
    return arr;
  }

  unsigned long i = 0;
  unsigned long k = 0;
  while (i < length) {
    int j = 0;
    while (i + j < length && j < delLength && str[i + j] == delimiter[j])
      j++;
    if (j == delLength)//found delimiter
    {
      arr.emplace_back(str.substr(k, i - k));
      i += delLength;
      k = i;
    } else {
      i++;
    }
  }
  arr.emplace_back(str.substr(k, i - k));
  return arr;
}

void Provider::parseTraceParent(const std::string &traceparent) {
  if (!traceparent.empty()) {
    std::vector<std::string> kv = explode("-", traceparent);
    if (kv.size() == 4) {
      auto span = firstOneSpan();
      span->set_trace_id(HexDecode(kv[1]).c_str(), 16);
      span->set_parent_span_id(HexDecode(kv[2]).c_str(), 8);
      //TODO setTraceFlags(kv[3]);
    }
  } else {
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist(1, OPENTELEMETRY_G(sample_ratio_based));
    if (OPENTELEMETRY_G(sample_ratio_based) == 1 || dist(mt) == OPENTELEMETRY_G(sample_ratio_based)) {
      //TODO setTraceFlags("01");
    } else if (OPENTELEMETRY_G(sample_ratio_based) == 0) {
      //TODO setTraceFlags("00");
    }
  }
}

void Provider::parseTraceState(const std::string &tracestate) {
  if (!tracestate.empty()) {
    for (const auto &item : explode(",", tracestate)) {
      std::vector<std::string> kv = explode("=", item);
      if (kv.size() == 2) {
        //TODO addTraceState(kv[0], kv[1]);
      }
    }
  }
}



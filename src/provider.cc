//
// Created by kilingzhang on 2021/6/15.
//

#include "php_opentelemetry.h"
#include "include/provider.h"
#include "include/utils.h"
#include <include/hex.h>

#include <utility>
#include <random>
#include <unistd.h>

#include "opentelemetry/proto/collector/trace/v1/trace_service.pb.h"

using namespace opentelemetry::proto::trace::v1;
using namespace opentelemetry::proto::collector::trace::v1;

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
      set_string_attribute(resource->add_attributes(), "net.host.ip", OPENTELEMETRY_G(ipv4));
      set_string_attribute(resource->add_attributes(), "net.host.name", name);
      set_string_attribute(resource->add_attributes(), "os.type", PLATFORM_NAME);
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
      set_string_attribute(resource->add_attributes(), "net.host.ip", OPENTELEMETRY_G(ipv4));
      set_string_attribute(resource->add_attributes(), "net.host.name", name);
      set_string_attribute(resource->add_attributes(), "os.type", PLATFORM_NAME);
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

bool Provider::isSampled() {
  if (!is_cli_sapi()) {
    if (is_sampled) {
      return true;
    }
  } else {
    pid_t ppid = get_current_ppid();
    if (sampled_map.find(ppid) != sampled_map.end()) {
      return sampled_map[ppid];
    }
  }
  return false;
}

void Provider::setSampled(bool isSampled) {
  if (!is_cli_sapi()) {
    is_sampled = isSampled;
  } else {
    pid_t ppid = get_current_ppid();
    sampled_map[ppid] = isSampled;
  }
};

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

void Provider::addTraceStates(const std::string &key, const std::string &value) {
  tsl::robin_map<std::string, std::string> traceStates;
  std::string statesHeader;
  if (!is_cli_sapi()) {
    states[key] = value;
    traceStates = states;
  } else {
    pid_t ppid = get_current_ppid();
    states_map[ppid][key] = value;
    traceStates = states_map[ppid];
  }
  auto iter = traceStates.begin();
  bool first = true;
  while (iter != traceStates.end()) {
    if (!first) {
      statesHeader = statesHeader.append(",");
    }
    first = false;
    statesHeader = statesHeader.append(iter->first);
    statesHeader = statesHeader.append("=");
    statesHeader = statesHeader.append(iter->second);
    iter++;
  }
  OPENTELEMETRY_G(provider)->firstOneSpan()->set_trace_state(statesHeader);
}

tsl::robin_map<std::string, std::string> Provider::getTraceStates() {
  if (!is_cli_sapi()) {
    return states;
  } else {
    pid_t ppid = get_current_ppid();
    if (states_map.find(ppid) != states_map.end()) {
      return states_map[ppid];
    }
  }
  return {};
}

void Provider::parseTraceParent(const std::string &traceparent) {
  if (!traceparent.empty()) {
    std::vector<std::string> kv = explode("-", traceparent);
    if (kv.size() == 4) {
      auto span = firstOneSpan();
      span->set_trace_id(Hex::decode(kv[1]).data(), 16);
      span->set_parent_span_id(Hex::decode(kv[2]).data(), 8);
      setSampled(kv[3] == "01");
    }
  } else {
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist(1, OPENTELEMETRY_G(sample_ratio_based));
    int d = dist(mt);
    if (OPENTELEMETRY_G(sample_ratio_based) == 1 || d == OPENTELEMETRY_G(sample_ratio_based)) {
      setSampled(true);
    } else if (OPENTELEMETRY_G(sample_ratio_based) == 0) {
      setSampled(false);
    }
  }
}

void Provider::parseTraceState(const std::string &tracestate) {
  if (!tracestate.empty()) {
    for (const auto &item : explode(",", tracestate)) {
      std::vector<std::string> kv = explode("=", item);
      if (kv.size() == 2) {
        addTraceStates(kv[0], kv[1]);
      }
    }
  }
}

std::string Provider::formatTraceParentHeader(Span *span) {
  return "00-" + traceId(*span) + "-" + spanId(*span) + "-" + (isSampled() ? "01" : "00");
}

std::string Provider::formatTraceStateHeader() {
  return OPENTELEMETRY_G(provider)->firstOneSpan()->trace_state();
}



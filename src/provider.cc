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
using namespace opentelemetry::proto::resource::v1;
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

Resource *Provider::getResource() {
    if (!is_cli_sapi()) {
        return resource;
    } else {
        pid_t ppid = get_current_ppid();
        return resources[ppid];
    }
}

ResourceSpans *Provider::getTracer() {
    if (!is_cli_sapi()) {
        if (!request) {
            request = new ExportTraceServiceRequest();
            resourceSpan = request->add_resource_spans();
            resource = resourceSpan->resource().New();
            resource->set_dropped_attributes_count(0);
            set_string_attribute(resource->add_attributes(), "service.name", OPENTELEMETRY_G(service_name));
            set_string_attribute(resource->add_attributes(), "net.host.ip", OPENTELEMETRY_G(ipv4));
            set_string_attribute(resource->add_attributes(), "net.host.name", OPENTELEMETRY_G(hostname));
            set_string_attribute(resource->add_attributes(), "os.type", PLATFORM_NAME);
            set_string_attribute(resource->add_attributes(), "process.pid", std::to_string(getpid()));
            set_string_attribute(resource->add_attributes(), "telemetry.sdk.language", "php");
            std::string resource_attributes = OPENTELEMETRY_G(resource_attributes);
            if (!resource_attributes.empty()) {
                for (const auto &item : explode(",", resource_attributes)) {
                    std::vector<std::string> kv = explode("=", item);
                    if (kv.size() == 2 && !kv[0].empty() && !kv[1].empty()) {
                        set_string_attribute(resource->add_attributes(), kv[0], kv[1]);
                    }
                }
            }
        }
        return resourceSpan;
    } else {
        pid_t ppid = get_current_ppid();
        if (requests.find(ppid) != requests.end()) {
            return resourceSpans[ppid];
        }
        requests[ppid] = new ExportTraceServiceRequest();
        resourceSpans[ppid] = requests[ppid]->add_resource_spans();
        resources[ppid] = resourceSpans[ppid]->resource().New();
        resources[ppid]->set_dropped_attributes_count(0);
        set_string_attribute(resources[ppid]->add_attributes(), "service.name", OPENTELEMETRY_G(service_name));
        set_string_attribute(resources[ppid]->add_attributes(), "net.host.ip", OPENTELEMETRY_G(ipv4));
        set_string_attribute(resources[ppid]->add_attributes(), "net.host.name", OPENTELEMETRY_G(hostname));
        set_string_attribute(resources[ppid]->add_attributes(), "os.type", PLATFORM_NAME);
        set_string_attribute(resources[ppid]->add_attributes(), "process.pid", std::to_string(getpid()));
        set_string_attribute(resources[ppid]->add_attributes(), "telemetry.sdk.language", "php");
        std::string resource_attributes = OPENTELEMETRY_G(resource_attributes);
        if (!resource_attributes.empty()) {
            for (const auto &item : explode(",", resource_attributes)) {
                std::vector<std::string> kv = explode("=", item);
                if (kv.size() == 2 && !kv[0].empty() && !kv[1].empty()) {
                    set_string_attribute(resources[ppid]->add_attributes(), kv[0], kv[1]);
                }
            }
        }
        return resourceSpans[ppid];
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
    span->set_dropped_events_count(0);
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
        if (sampled_map.find(ppid) != sampled_map.end() && sampled_map[ppid]) {
            return true;
        }
    }

    unsigned long time_consuming =
        get_unix_nanoseconds() - OPENTELEMETRY_G(provider)->firstOneSpan()->start_time_unix_nano();
    time_consuming /= 1e6;
    return time_consuming >= OPENTELEMETRY_G(max_time_consuming);
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
            resourceSpan = nullptr;
            firstSpan = nullptr;
            request->Clear();
            delete request;
            request = nullptr;
            is_sampled = false;
            baggage.clear();
            redisException.clear();
            redisException.shrink_to_fit();
        }
    } else {
        pid_t ppid = get_current_ppid();
        if (requests.find(ppid) != requests.end()) {
            sampled_map.erase(ppid);
            firstSpans.erase(ppid);
            resourceSpans.erase(ppid);
            requests[ppid]->Clear();
            delete requests[ppid];
            requests.erase(ppid);
        }
        if (baggage_map.find(ppid) != baggage_map.end()) {
            baggage_map[ppid].clear();
        }
        if (redisExceptions.find(ppid) != redisExceptions.end()) {
            redisExceptions.erase(ppid);
        }
    }
}

void Provider::addBaggage(const std::string &key, const std::string &value) {
    if (!is_cli_sapi()) {
        baggage[key] = value;
    } else {
        pid_t ppid = get_current_ppid();
        baggage_map[ppid][key] = value;
    }
    set_string_attribute(getResource()->add_attributes(), key, value);
}

tsl::robin_map<std::string, std::string> Provider::getBaggage() {
    if (!is_cli_sapi()) {
        return baggage;
    } else {
        pid_t ppid = get_current_ppid();
        if (baggage_map.find(ppid) == baggage_map.end()) {
            baggage_map[ppid] = tsl::robin_map<std::string, std::string>();
        }
        return baggage_map[ppid];
    }
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
        if (OPENTELEMETRY_G(sample_ratio_based) == 1 ||
            OPENTELEMETRY_G(provider)->count() % OPENTELEMETRY_G(sample_ratio_based) == 0) {
            setSampled(true);
        } else if (OPENTELEMETRY_G(sample_ratio_based) == 0) {
            setSampled(false);
        }
    }
}

void Provider::parseBaggage(const std::string &tracebaggage) {
    if (!tracebaggage.empty()) {
        for (const auto &item : explode(",", tracebaggage)) {
            std::vector<std::string> kv = explode("=", item);
            if (kv.size() == 2) {
                addBaggage(kv[0], kv[1]);
            }
        }
    }
}

std::string Provider::formatTraceParentHeader(Span *span) {
    return "00-" + traceId(*span) + "-" + spanId(*span) + "-" + (isSampled() ? "01" : "00");
}

std::string Provider::formatBaggageHeader() {
    tsl::robin_map<std::string, std::string> baggageValue;
    std::string baggageHeader;
    if (!is_cli_sapi()) {
        baggageValue = baggage;
    } else {
        pid_t ppid = get_current_ppid();
        baggageValue = baggage_map[ppid];
    }
    auto iter = baggageValue.begin();
    bool first = true;
    while (iter != baggageValue.end()) {
        if (!first) {
            baggageHeader = baggageHeader.append(",");
        }
        first = false;
        baggageHeader = baggageHeader.append(iter->first);
        baggageHeader = baggageHeader.append("=");
        baggageHeader = baggageHeader.append(iter->second);
        iter++;
    }
    return baggageHeader;
}

void Provider::okEnd(Span *span) {
    if (span->status().code() == Status_StatusCode_STATUS_CODE_UNSET) {
        auto status = new Status();
        status->set_code(Status_StatusCode_STATUS_CODE_OK);
        span->set_allocated_status(status);
    }
    span->set_end_time_unix_nano(get_unix_nanoseconds());
}

void Provider::errorEnd(Span *span, const std::string &message) {
    auto status = new Status();
    status->set_code(Status_StatusCode_STATUS_CODE_ERROR);
    status->set_message(message);
    span->set_allocated_status(status);
    span->set_end_time_unix_nano(get_unix_nanoseconds());
    OPENTELEMETRY_G(provider)->setSampled(true);
}

void Provider::increase() {
    if (counter > 10000000) {
        counter = 0;
    }
    counter++;
}

long Provider::count() const {
    return counter;
}

std::string Provider::getRedisException() {
    if (!is_cli_sapi()) {
        return redisException;
    } else {
        pid_t ppid = get_current_ppid();
        if (redisExceptions.find(ppid) != redisExceptions.end() && !redisExceptions[ppid].empty()) {
            return redisExceptions[ppid];
        }
    }
    return "";
}

void Provider::setRedisException(const std::string &message) {
    if (!is_cli_sapi()) {
        redisException = message;
    } else {
        pid_t ppid = get_current_ppid();
        redisExceptions[ppid] = message;
    }
}

void Provider::clearRedisException() {
    setRedisException("");
}

bool Provider::isRedisException() {
    return !getRedisException().empty();
}

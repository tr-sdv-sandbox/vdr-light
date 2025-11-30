/*
 * encoder.cpp - Simulated MQTT encoder for VDR
 */

#include "vdr/encoder.hpp"

#include <glog/logging.h>

namespace vdr {

nlohmann::json Encoder::encode_header(const telemetry_Header& header) {
    return {
        {"source_id", header.source_id ? header.source_id : ""},
        {"timestamp_ns", header.timestamp_ns},
        {"seq_num", header.seq_num},
        {"correlation_id", header.correlation_id ? header.correlation_id : ""}
    };
}

void Encoder::log_mqtt_publish(const std::string& topic, const nlohmann::json& payload) {
    LOG(INFO) << "[MQTT] topic=" << topic << " payload=" << payload.dump();
}

void Encoder::send(const telemetry_vss_Signal& msg) {
    nlohmann::json payload = {
        {"header", encode_header(msg.header)},
        {"path", msg.path ? msg.path : ""},
        {"quality", static_cast<int>(msg.quality)},
        {"value_type", static_cast<int>(msg.value_type)}
    };

    // Add value based on type
    switch (msg.value_type) {
        case telemetry_vss_VALUE_TYPE_BOOL:
            payload["value"] = msg.bool_value;
            break;
        case telemetry_vss_VALUE_TYPE_INT32:
            payload["value"] = msg.int32_value;
            break;
        case telemetry_vss_VALUE_TYPE_INT64:
            payload["value"] = msg.int64_value;
            break;
        case telemetry_vss_VALUE_TYPE_FLOAT:
            payload["value"] = msg.float_value;
            break;
        case telemetry_vss_VALUE_TYPE_DOUBLE:
            payload["value"] = msg.double_value;
            break;
        case telemetry_vss_VALUE_TYPE_STRING:
            payload["value"] = msg.string_value ? msg.string_value : "";
            break;
    }

    log_mqtt_publish("v1/vss/signals", payload);
}

void Encoder::send(const telemetry_events_Event& msg) {
    nlohmann::json payload = {
        {"header", encode_header(msg.header)},
        {"event_id", msg.event_id ? msg.event_id : ""},
        {"category", msg.category ? msg.category : ""},
        {"event_type", msg.event_type ? msg.event_type : ""},
        {"severity", static_cast<int>(msg.severity)}
    };

    // Encode payload as base64 or hex (simplified: just size for now)
    if (msg.payload._length > 0) {
        payload["payload_size"] = msg.payload._length;
        // In production: base64 encode msg.payload._buffer
    }

    log_mqtt_publish("v1/events", payload);
}

void Encoder::send(const telemetry_metrics_Gauge& msg) {
    nlohmann::json labels = nlohmann::json::object();
    for (uint32_t i = 0; i < msg.labels._length; ++i) {
        const auto& kv = msg.labels._buffer[i];
        if (kv.key && kv.value) {
            labels[kv.key] = kv.value;
        }
    }

    nlohmann::json payload = {
        {"header", encode_header(msg.header)},
        {"name", msg.name ? msg.name : ""},
        {"labels", labels},
        {"value", msg.value}
    };

    log_mqtt_publish("v1/telemetry/gauges", payload);
}

void Encoder::send(const telemetry_metrics_Counter& msg) {
    nlohmann::json labels = nlohmann::json::object();
    for (uint32_t i = 0; i < msg.labels._length; ++i) {
        const auto& kv = msg.labels._buffer[i];
        if (kv.key && kv.value) {
            labels[kv.key] = kv.value;
        }
    }

    nlohmann::json payload = {
        {"header", encode_header(msg.header)},
        {"name", msg.name ? msg.name : ""},
        {"labels", labels},
        {"value", msg.value}
    };

    log_mqtt_publish("v1/telemetry/counters", payload);
}

void Encoder::send(const telemetry_metrics_Histogram& msg) {
    nlohmann::json labels = nlohmann::json::object();
    for (uint32_t i = 0; i < msg.labels._length; ++i) {
        const auto& kv = msg.labels._buffer[i];
        if (kv.key && kv.value) {
            labels[kv.key] = kv.value;
        }
    }

    nlohmann::json buckets = nlohmann::json::array();
    for (uint32_t i = 0; i < msg.buckets._length; ++i) {
        const auto& bucket = msg.buckets._buffer[i];
        buckets.push_back({
            {"upper_bound", bucket.upper_bound},
            {"cumulative_count", bucket.cumulative_count}
        });
    }

    nlohmann::json payload = {
        {"header", encode_header(msg.header)},
        {"name", msg.name ? msg.name : ""},
        {"labels", labels},
        {"sample_count", msg.sample_count},
        {"sample_sum", msg.sample_sum},
        {"buckets", buckets}
    };

    log_mqtt_publish("v1/telemetry/histograms", payload);
}

void Encoder::send(const telemetry_logs_LogEntry& msg) {
    nlohmann::json fields = nlohmann::json::object();
    for (uint32_t i = 0; i < msg.fields._length; ++i) {
        const auto& kv = msg.fields._buffer[i];
        if (kv.key && kv.value) {
            fields[kv.key] = kv.value;
        }
    }

    nlohmann::json payload = {
        {"header", encode_header(msg.header)},
        {"level", static_cast<int>(msg.level)},
        {"component", msg.component ? msg.component : ""},
        {"message", msg.message ? msg.message : ""},
        {"fields", fields}
    };

    log_mqtt_publish("v1/logs", payload);
}

void Encoder::send(const telemetry_diagnostics_ScalarMeasurement& msg) {
    nlohmann::json payload = {
        {"header", encode_header(msg.header)},
        {"variable_id", msg.variable_id ? msg.variable_id : ""},
        {"unit", msg.unit ? msg.unit : ""},
        {"mtype", static_cast<int>(msg.mtype)},
        {"value", msg.value}
    };

    log_mqtt_publish("v1/diagnostics/scalar", payload);
}

void Encoder::send(const telemetry_diagnostics_VectorMeasurement& msg) {
    nlohmann::json values = nlohmann::json::array();
    for (uint32_t i = 0; i < msg.values._length; ++i) {
        values.push_back(msg.values._buffer[i]);
    }

    nlohmann::json payload = {
        {"header", encode_header(msg.header)},
        {"variable_id", msg.variable_id ? msg.variable_id : ""},
        {"unit", msg.unit ? msg.unit : ""},
        {"mtype", static_cast<int>(msg.mtype)},
        {"values", values}
    };

    log_mqtt_publish("v1/diagnostics/vector", payload);
}

}  // namespace vdr

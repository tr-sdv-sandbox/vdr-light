#pragma once

/*
 * encoder.hpp - Simulated MQTT encoder for VDR
 *
 * In production, this would encode data and publish to MQTT.
 * For the PoC, it logs what would be sent using glog.
 */

#include "telemetry.h"

#include <nlohmann/json.hpp>

#include <string>

namespace vdr {

/*
 * Encoder - converts DDS messages to JSON (simulating MQTT payload).
 *
 * In production:
 * - Would use a compact binary format (protobuf, msgpack, etc.)
 * - Would batch messages
 * - Would publish to MQTT
 *
 * For PoC:
 * - Encodes to JSON
 * - Logs via glog
 */
class Encoder {
public:
    Encoder() = default;

    // Encode and "send" (log) messages
    void send(const telemetry_vss_Signal& msg);
    void send(const telemetry_events_Event& msg);
    void send(const telemetry_metrics_Gauge& msg);
    void send(const telemetry_metrics_Counter& msg);
    void send(const telemetry_metrics_Histogram& msg);
    void send(const telemetry_logs_LogEntry& msg);
    void send(const telemetry_diagnostics_ScalarMeasurement& msg);
    void send(const telemetry_diagnostics_VectorMeasurement& msg);

private:
    // Convert header to JSON
    nlohmann::json encode_header(const telemetry_Header& header);

    // Log the encoded message (simulates MQTT publish)
    void log_mqtt_publish(const std::string& topic, const nlohmann::json& payload);
};

}  // namespace vdr

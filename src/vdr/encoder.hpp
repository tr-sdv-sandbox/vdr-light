// Copyright 2025 VDR-Light Contributors
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

/// @file encoder.hpp
/// @brief Simulated MQTT encoder for VDR
///
/// In production, this would encode data and publish to MQTT.
/// For the PoC, it logs what would be sent using glog.

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

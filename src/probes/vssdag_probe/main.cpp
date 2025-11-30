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

/// @file vssdag_probe/main.cpp
/// @brief CAN-to-VSS Probe using libvssdag
///
/// Transforms raw CAN signals into VSS format using a DAG-based pipeline
/// with Lua scripting for transforms, then publishes to DDS.
///
/// Features:
/// - DBC parsing for CAN message decoding
/// - Topological sorting for derived signal dependencies
/// - Lua-based transforms (filters, calculations, state machines)
/// - Quality tracking (VALID, INVALID, NOT_AVAILABLE)
/// - Configurable via YAML mapping files

#include "common/dds_wrapper.hpp"
#include "common/qos_profiles.hpp"
#include "common/time_utils.hpp"
#include "telemetry.h"

#include <vssdag/signal_processor.h>
#include <vssdag/can/can_source.h>
#include <vssdag/mapping_types.h>
#include <vssdag/lua_mapper.h>
#include <vss/types/types.hpp>

#include <glog/logging.h>
#include <yaml-cpp/yaml.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

namespace {

std::atomic<bool> g_running{true};

void signal_handler(int signum) {
    LOG(INFO) << "Received signal " << signum << ", shutting down...";
    g_running = false;
}

// Convert vss::types::SignalQuality to our DDS Quality enum
telemetry_vss_Quality convert_quality(vss::types::SignalQuality quality) {
    switch (quality) {
        case vss::types::SignalQuality::VALID:
            return telemetry_vss_QUALITY_VALID;
        case vss::types::SignalQuality::INVALID:
            return telemetry_vss_QUALITY_INVALID;
        case vss::types::SignalQuality::NOT_AVAILABLE:
        default:
            return telemetry_vss_QUALITY_NOT_AVAILABLE;
    }
}

// Convert vss::types::Value to DDS signal fields
// Returns true if conversion succeeded, false if type not supported
bool set_value_fields(telemetry_vss_Signal& msg, const vss::types::Value& value,
                      std::string& string_buf) {
    // Check each type in the variant
    if (std::holds_alternative<bool>(value)) {
        msg.value_type = telemetry_vss_VALUE_TYPE_BOOL;
        msg.bool_value = std::get<bool>(value);
        return true;
    }
    if (std::holds_alternative<int32_t>(value)) {
        msg.value_type = telemetry_vss_VALUE_TYPE_INT32;
        msg.int32_value = std::get<int32_t>(value);
        return true;
    }
    if (std::holds_alternative<int64_t>(value)) {
        msg.value_type = telemetry_vss_VALUE_TYPE_INT64;
        msg.int64_value = std::get<int64_t>(value);
        return true;
    }
    if (std::holds_alternative<float>(value)) {
        msg.value_type = telemetry_vss_VALUE_TYPE_FLOAT;
        msg.float_value = std::get<float>(value);
        return true;
    }
    if (std::holds_alternative<double>(value)) {
        msg.value_type = telemetry_vss_VALUE_TYPE_DOUBLE;
        msg.double_value = std::get<double>(value);
        return true;
    }
    if (std::holds_alternative<std::string>(value)) {
        msg.value_type = telemetry_vss_VALUE_TYPE_STRING;
        string_buf = std::get<std::string>(value);
        msg.string_value = const_cast<char*>(string_buf.c_str());
        return true;
    }
    // Handle smaller integer types by promoting
    if (std::holds_alternative<int8_t>(value)) {
        msg.value_type = telemetry_vss_VALUE_TYPE_INT32;
        msg.int32_value = std::get<int8_t>(value);
        return true;
    }
    if (std::holds_alternative<int16_t>(value)) {
        msg.value_type = telemetry_vss_VALUE_TYPE_INT32;
        msg.int32_value = std::get<int16_t>(value);
        return true;
    }
    if (std::holds_alternative<uint8_t>(value)) {
        msg.value_type = telemetry_vss_VALUE_TYPE_INT32;
        msg.int32_value = std::get<uint8_t>(value);
        return true;
    }
    if (std::holds_alternative<uint16_t>(value)) {
        msg.value_type = telemetry_vss_VALUE_TYPE_INT32;
        msg.int32_value = std::get<uint16_t>(value);
        return true;
    }
    if (std::holds_alternative<uint32_t>(value)) {
        msg.value_type = telemetry_vss_VALUE_TYPE_INT64;
        msg.int64_value = std::get<uint32_t>(value);
        return true;
    }
    if (std::holds_alternative<uint64_t>(value)) {
        msg.value_type = telemetry_vss_VALUE_TYPE_INT64;
        msg.int64_value = static_cast<int64_t>(std::get<uint64_t>(value));
        return true;
    }

    // Unsupported type (monostate, arrays, structs)
    return false;
}

// Parse ValueType from string
vss::types::ValueType parse_datatype(const std::string& dtype) {
    if (dtype == "bool") return vss::types::ValueType::BOOL;
    if (dtype == "int8") return vss::types::ValueType::INT8;
    if (dtype == "int16") return vss::types::ValueType::INT16;
    if (dtype == "int32") return vss::types::ValueType::INT32;
    if (dtype == "int64") return vss::types::ValueType::INT64;
    if (dtype == "uint8") return vss::types::ValueType::UINT8;
    if (dtype == "uint16") return vss::types::ValueType::UINT16;
    if (dtype == "uint32") return vss::types::ValueType::UINT32;
    if (dtype == "uint64") return vss::types::ValueType::UINT64;
    if (dtype == "float") return vss::types::ValueType::FLOAT;
    if (dtype == "double") return vss::types::ValueType::DOUBLE;
    if (dtype == "string") return vss::types::ValueType::STRING;
    return vss::types::ValueType::UNSPECIFIED;
}

// Load signal mappings from YAML file
std::unordered_map<std::string, vssdag::SignalMapping> load_mappings(
    const std::string& yaml_path) {

    std::unordered_map<std::string, vssdag::SignalMapping> mappings;

    YAML::Node config = YAML::LoadFile(yaml_path);

    if (!config["signals"]) {
        LOG(WARNING) << "No 'signals' section in config";
        return mappings;
    }

    for (const auto& sig : config["signals"]) {
        vssdag::SignalMapping mapping;

        std::string signal_name = sig["signal"].as<std::string>();

        // Data type
        if (sig["datatype"]) {
            std::string dtype = sig["datatype"].as<std::string>();
            mapping.datatype = parse_datatype(dtype);
        }

        // Source configuration
        if (sig["source"]) {
            auto source = sig["source"];
            mapping.source.type = source["type"].as<std::string>("dbc");
            mapping.source.name = source["name"].as<std::string>("");
        }

        // Dependencies
        if (sig["depends_on"]) {
            for (const auto& dep : sig["depends_on"]) {
                mapping.depends_on.push_back(dep.as<std::string>());
            }
        }

        // Transform
        if (sig["transform"]) {
            auto transform = sig["transform"];
            if (transform["code"]) {
                vssdag::CodeTransform code_transform;
                code_transform.expression = transform["code"].as<std::string>();
                mapping.transform = code_transform;
            } else if (transform["value_map"]) {
                vssdag::ValueMapping value_map;
                for (const auto& kv : transform["value_map"]) {
                    value_map.mappings[kv.first.as<std::string>()] =
                        kv.second.as<std::string>();
                }
                mapping.transform = value_map;
            }
        }

        // Throttling
        if (sig["interval_ms"]) {
            mapping.interval_ms = sig["interval_ms"].as<int>();
        }

        // Update trigger
        if (sig["update_trigger"]) {
            std::string trigger = sig["update_trigger"].as<std::string>();
            if (trigger == "periodic") {
                mapping.update_trigger = vssdag::UpdateTrigger::PERIODIC;
            } else if (trigger == "both") {
                mapping.update_trigger = vssdag::UpdateTrigger::BOTH;
            } else {
                mapping.update_trigger = vssdag::UpdateTrigger::ON_DEPENDENCY;
            }
        }

        mappings[signal_name] = std::move(mapping);
    }

    LOG(INFO) << "Loaded " << mappings.size() << " signal mappings";
    return mappings;
}

}  // namespace

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);
    google::SetStderrLogging(google::INFO);
    FLAGS_colorlogtostderr = true;

    LOG(INFO) << "VSS DAG Probe starting...";

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Parse command line arguments
    std::string config_path = "config/vssdag_probe_config.yaml";
    std::string can_interface = "vcan0";
    std::string dbc_path = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--interface" && i + 1 < argc) {
            can_interface = argv[++i];
        } else if (arg == "--dbc" && i + 1 < argc) {
            dbc_path = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "  --config PATH     Signal mappings YAML file\n"
                      << "  --interface NAME  CAN interface (default: vcan0)\n"
                      << "  --dbc PATH        DBC file for CAN decoding\n"
                      << "  --help            Show this help\n";
            return 0;
        }
    }

    try {
        // Load configuration
        auto mappings = load_mappings(config_path);
        if (mappings.empty()) {
            LOG(ERROR) << "No signal mappings loaded from " << config_path;
            return 1;
        }

        // Create signal processor DAG
        vssdag::SignalProcessorDAG processor;
        if (!processor.initialize(mappings)) {
            LOG(ERROR) << "Failed to initialize signal processor";
            return 1;
        }

        LOG(INFO) << "Signal processor initialized with " << mappings.size()
                  << " mappings";

        // Create CAN signal source (if DBC provided)
        std::unique_ptr<vssdag::CANSignalSource> can_source;
        if (!dbc_path.empty()) {
            can_source = std::make_unique<vssdag::CANSignalSource>(
                can_interface, dbc_path, mappings);

            if (!can_source->initialize()) {
                LOG(ERROR) << "Failed to initialize CAN source on " << can_interface;
                return 1;
            }

            LOG(INFO) << "CAN source initialized: " << can_interface
                      << " with DBC: " << dbc_path;
        } else {
            LOG(WARNING) << "No DBC file specified, running in simulation mode";
        }

        // Create DDS participant and writer
        dds::Participant participant(DDS_DOMAIN_DEFAULT);

        auto qos = dds::qos_profiles::reliable_standard(100);
        dds::Topic topic(participant, &telemetry_vss_Signal_desc,
                         "rt/vss/signals", qos.get());
        dds::Writer writer(participant, topic, qos.get());

        LOG(INFO) << "DDS writer created for rt/vss/signals";
        LOG(INFO) << "VSS DAG Probe ready. Press Ctrl+C to stop.";

        uint32_t seq = 0;
        uint64_t signals_published = 0;

        // Buffers for string storage
        std::string source_id = "vssdag_probe";
        std::string correlation_id = "";
        std::vector<std::string> path_buffers;
        std::vector<std::string> string_value_buffers;

        while (g_running) {
            std::vector<vssdag::SignalUpdate> updates;

            // Poll CAN source for new signals
            if (can_source) {
                updates = can_source->poll();
            } else {
                // Simulation mode - generate test signals
                static auto last_sim = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();

                if (now - last_sim >= std::chrono::milliseconds(100)) {
                    last_sim = now;

                    // Simulate speed signal
                    static double sim_speed = 0.0;
                    sim_speed += 0.5;
                    if (sim_speed > 120.0) sim_speed = 0.0;

                    vssdag::SignalUpdate speed_update;
                    speed_update.signal_name = "CAN.VehicleSpeed";
                    speed_update.value = sim_speed;
                    speed_update.timestamp = now;
                    speed_update.status = vss::types::SignalQuality::VALID;
                    updates.push_back(speed_update);

                    // Simulate SOC signal
                    static double sim_soc = 80.0;
                    sim_soc -= 0.01;
                    if (sim_soc < 10.0) sim_soc = 100.0;

                    vssdag::SignalUpdate soc_update;
                    soc_update.signal_name = "CAN.BatterySOC";
                    soc_update.value = sim_soc;
                    soc_update.timestamp = now;
                    soc_update.status = vss::types::SignalQuality::VALID;
                    updates.push_back(soc_update);
                }
            }

            // Process through DAG (transforms, filters, derived signals)
            if (!updates.empty()) {
                auto vss_signals = processor.process_signal_updates(updates);

                // Prepare buffers
                path_buffers.resize(vss_signals.size());
                string_value_buffers.resize(vss_signals.size());

                // Publish each output signal to DDS
                for (size_t i = 0; i < vss_signals.size(); ++i) {
                    const auto& sig = vss_signals[i];

                    // Only publish valid signals
                    if (sig.qualified_value.quality != vss::types::SignalQuality::VALID) {
                        continue;
                    }

                    telemetry_vss_Signal msg = {};

                    // Store path in buffer
                    path_buffers[i] = sig.path;
                    msg.path = const_cast<char*>(path_buffers[i].c_str());

                    // Header
                    msg.header.source_id = const_cast<char*>(source_id.c_str());
                    msg.header.timestamp_ns = utils::now_ns();
                    msg.header.seq_num = seq++;
                    msg.header.correlation_id = const_cast<char*>(correlation_id.c_str());

                    // Quality
                    msg.quality = convert_quality(sig.qualified_value.quality);

                    // Value
                    if (!set_value_fields(msg, sig.qualified_value.value, string_value_buffers[i])) {
                        LOG(WARNING) << "Unsupported value type for signal: " << sig.path;
                        continue;
                    }

                    writer.write(msg);
                    ++signals_published;
                }
            }

            LOG_EVERY_N(INFO, 1000) << "Signals published: " << signals_published;

            // Small sleep to avoid busy-waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Cleanup
        if (can_source) {
            can_source->stop();
        }

        LOG(INFO) << "VSS DAG Probe shutdown. Total signals published: "
                  << signals_published;

    } catch (const YAML::Exception& e) {
        LOG(FATAL) << "YAML error: " << e.what();
        return 1;
    } catch (const dds::Error& e) {
        LOG(FATAL) << "DDS error: " << e.what();
        return 1;
    } catch (const std::exception& e) {
        LOG(FATAL) << "Error: " << e.what();
        return 1;
    }

    google::ShutdownGoogleLogging();
    return 0;
}

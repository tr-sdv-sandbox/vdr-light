/*
 * vdr/main.cpp - Vehicle Data Readout main entry point
 *
 * VDR subscribes to DDS topics and forwards data for offboarding.
 * In this PoC, "offboarding" means logging what would be sent via MQTT.
 */

#include "common/dds_wrapper.hpp"
#include "vdr/subscriber.hpp"
#include "vdr/encoder.hpp"

#include <glog/logging.h>
#include <yaml-cpp/yaml.h>

#include <csignal>
#include <atomic>
#include <iostream>

namespace {

std::atomic<bool> g_running{true};

void signal_handler(int signum) {
    LOG(INFO) << "Received signal " << signum << ", shutting down...";
    g_running = false;
}

vdr::SubscriptionConfig load_config(const std::string& config_path) {
    vdr::SubscriptionConfig config;

    try {
        YAML::Node yaml = YAML::LoadFile(config_path);

        if (yaml["subscriptions"]) {
            for (const auto& sub : yaml["subscriptions"]) {
                std::string topic = sub["topic"].as<std::string>("");
                bool enabled = sub["enabled"].as<bool>(true);

                if (topic == "rt/vss/signals") {
                    config.vss_signals = enabled;
                } else if (topic == "rt/events/vehicle") {
                    config.events = enabled;
                } else if (topic == "rt/telemetry/gauges") {
                    config.gauges = enabled;
                } else if (topic == "rt/telemetry/counters") {
                    config.counters = enabled;
                } else if (topic == "rt/telemetry/histograms") {
                    config.histograms = enabled;
                } else if (topic == "rt/logs/entries") {
                    config.logs = enabled;
                } else if (topic == "rt/diagnostics/scalar") {
                    config.scalar_measurements = enabled;
                } else if (topic == "rt/diagnostics/vector") {
                    config.vector_measurements = enabled;
                }
            }
        }

        LOG(INFO) << "Loaded configuration from " << config_path;
    } catch (const YAML::Exception& e) {
        LOG(WARNING) << "Failed to load config from " << config_path
                     << ": " << e.what() << ". Using defaults.";
    }

    return config;
}

}  // namespace

int main(int argc, char* argv[]) {
    // Initialize logging
    google::InitGoogleLogging(argv[0]);
    google::SetStderrLogging(google::INFO);
    FLAGS_colorlogtostderr = true;

    LOG(INFO) << "VDR (Vehicle Data Readout) starting...";

    // Signal handling
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Load configuration
    std::string config_path = "config/vdr_config.yaml";
    if (argc > 1) {
        config_path = argv[1];
    }

    auto config = load_config(config_path);

    // Log configuration
    LOG(INFO) << "Subscription config:";
    LOG(INFO) << "  vss_signals: " << (config.vss_signals ? "enabled" : "disabled");
    LOG(INFO) << "  events: " << (config.events ? "enabled" : "disabled");
    LOG(INFO) << "  gauges: " << (config.gauges ? "enabled" : "disabled");
    LOG(INFO) << "  counters: " << (config.counters ? "enabled" : "disabled");
    LOG(INFO) << "  histograms: " << (config.histograms ? "enabled" : "disabled");
    LOG(INFO) << "  logs: " << (config.logs ? "enabled" : "disabled");
    LOG(INFO) << "  scalar_measurements: " << (config.scalar_measurements ? "enabled" : "disabled");
    LOG(INFO) << "  vector_measurements: " << (config.vector_measurements ? "enabled" : "disabled");

    try {
        // Create DDS participant
        dds::Participant participant(DDS_DOMAIN_DEFAULT);

        // Create encoder (simulated MQTT publisher)
        vdr::Encoder encoder;

        // Create subscription manager
        vdr::SubscriptionManager subscriptions(participant, config);

        // Register callbacks - each forwards to encoder
        subscriptions.on_vss_signal([&encoder](const telemetry_vss_Signal& msg) {
            encoder.send(msg);
        });

        subscriptions.on_event([&encoder](const telemetry_events_Event& msg) {
            encoder.send(msg);
        });

        subscriptions.on_gauge([&encoder](const telemetry_metrics_Gauge& msg) {
            encoder.send(msg);
        });

        subscriptions.on_counter([&encoder](const telemetry_metrics_Counter& msg) {
            encoder.send(msg);
        });

        subscriptions.on_histogram([&encoder](const telemetry_metrics_Histogram& msg) {
            encoder.send(msg);
        });

        subscriptions.on_log_entry([&encoder](const telemetry_logs_LogEntry& msg) {
            encoder.send(msg);
        });

        subscriptions.on_scalar_measurement([&encoder](const telemetry_diagnostics_ScalarMeasurement& msg) {
            encoder.send(msg);
        });

        subscriptions.on_vector_measurement([&encoder](const telemetry_diagnostics_VectorMeasurement& msg) {
            encoder.send(msg);
        });

        // Start receiving
        subscriptions.start();

        LOG(INFO) << "VDR running. Press Ctrl+C to stop.";

        // Main loop - just wait for signal
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Stop subscriptions
        subscriptions.stop();

        LOG(INFO) << "VDR shutdown complete.";

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

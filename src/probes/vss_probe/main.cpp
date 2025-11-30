/*
 * vss_probe/main.cpp - Sample VSS signal probe
 *
 * Simulates a probe that samples VSS signals and publishes to DDS.
 * In production, this would read from Kuksa broker or similar.
 */

#include "common/dds_wrapper.hpp"
#include "common/qos_profiles.hpp"
#include "common/time_utils.hpp"
#include "telemetry.h"

#include <glog/logging.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cmath>
#include <random>
#include <thread>

namespace {

std::atomic<bool> g_running{true};

void signal_handler(int signum) {
    LOG(INFO) << "Received signal " << signum << ", shutting down...";
    g_running = false;
}

// Simulated VSS signal paths
const char* VSS_PATHS[] = {
    "Vehicle.Speed",
    "Vehicle.Powertrain.TractionBattery.StateOfCharge.Current",
    "Vehicle.Powertrain.ElectricMotor.Temperature",
    "Vehicle.Cabin.HVAC.AmbientAirTemperature",
    "Vehicle.CurrentLocation.Latitude",
    "Vehicle.CurrentLocation.Longitude",
    "Vehicle.Chassis.SteeringWheel.Angle"
};
constexpr size_t NUM_PATHS = sizeof(VSS_PATHS) / sizeof(VSS_PATHS[0]);

}  // namespace

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);
    google::SetStderrLogging(google::INFO);
    FLAGS_colorlogtostderr = true;

    LOG(INFO) << "VSS Probe starting...";

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Parse publish rate (Hz)
    double rate_hz = 10.0;  // Default 10 Hz
    if (argc > 1) {
        rate_hz = std::stod(argv[1]);
    }
    auto interval = std::chrono::milliseconds(static_cast<int>(1000.0 / rate_hz));
    LOG(INFO) << "Publishing at " << rate_hz << " Hz (interval: "
              << interval.count() << " ms)";

    try {
        // Create DDS participant
        dds::Participant participant(DDS_DOMAIN_DEFAULT);

        // Create topic with reliable QoS
        auto qos = dds::qos_profiles::reliable_standard(100);
        dds::Topic topic(participant, &telemetry_vss_Signal_desc,
                         "rt/vss/signals", qos.get());

        // Create writer
        dds::Writer writer(participant, topic, qos.get());

        LOG(INFO) << "VSS Probe ready. Press Ctrl+C to stop.";

        // Random number generator for simulated values
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> speed_dist(0.0, 150.0);
        std::uniform_real_distribution<> soc_dist(0.0, 100.0);
        std::uniform_real_distribution<> temp_dist(-20.0, 80.0);
        std::uniform_real_distribution<> lat_dist(55.0, 70.0);
        std::uniform_real_distribution<> lon_dist(10.0, 25.0);
        std::uniform_real_distribution<> angle_dist(-720.0, 720.0);

        uint32_t sequence = 0;
        double sim_time = 0.0;

        while (g_running) {
            auto start = std::chrono::steady_clock::now();

            // Simulate sinusoidal speed variation
            double speed = 50.0 + 40.0 * std::sin(sim_time * 0.1);
            double soc = 80.0 - (sim_time * 0.01);  // Slowly decreasing
            if (soc < 10.0) soc = 80.0;  // Reset

            // Publish each signal
            for (size_t i = 0; i < NUM_PATHS; ++i) {
                telemetry_vss_Signal msg = {};

                // Set path (key)
                msg.path = const_cast<char*>(VSS_PATHS[i]);

                // Set header
                msg.header.source_id = const_cast<char*>("vss_probe");
                msg.header.timestamp_ns = utils::now_ns();
                msg.header.seq_num = sequence++;
                msg.header.correlation_id = const_cast<char*>("");

                // Set quality
                msg.quality = telemetry_vss_QUALITY_VALID;

                // Set value based on path
                if (i == 0) {  // Vehicle.Speed
                    msg.value_type = telemetry_vss_VALUE_TYPE_DOUBLE;
                    msg.double_value = speed;
                } else if (i == 1) {  // SOC
                    msg.value_type = telemetry_vss_VALUE_TYPE_DOUBLE;
                    msg.double_value = soc;
                } else if (i == 2) {  // Motor temp
                    msg.value_type = telemetry_vss_VALUE_TYPE_DOUBLE;
                    msg.double_value = 45.0 + speed_dist(gen) * 0.2;
                } else if (i == 3) {  // Ambient temp
                    msg.value_type = telemetry_vss_VALUE_TYPE_DOUBLE;
                    msg.double_value = 15.0 + temp_dist(gen) * 0.1;
                } else if (i == 4) {  // Latitude
                    msg.value_type = telemetry_vss_VALUE_TYPE_DOUBLE;
                    msg.double_value = 59.3293 + std::sin(sim_time * 0.01) * 0.01;
                } else if (i == 5) {  // Longitude
                    msg.value_type = telemetry_vss_VALUE_TYPE_DOUBLE;
                    msg.double_value = 18.0686 + std::cos(sim_time * 0.01) * 0.01;
                } else {  // Steering angle
                    msg.value_type = telemetry_vss_VALUE_TYPE_DOUBLE;
                    msg.double_value = angle_dist(gen);
                }

                writer.write(msg);
            }

            sim_time += interval.count() / 1000.0;

            // Sleep for remainder of interval
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed < interval) {
                std::this_thread::sleep_for(interval - elapsed);
            }
        }

        LOG(INFO) << "VSS Probe shutdown. Total samples published: " << sequence;

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

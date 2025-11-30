#pragma once

/*
 * time_utils.hpp - Time utilities for VDR ecosystem
 */

#include <chrono>
#include <cstdint>
#include <string>

namespace utils {

/*
 * Get current time as nanoseconds since epoch.
 */
inline int64_t now_ns() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

/*
 * Get current time as milliseconds since epoch.
 */
inline int64_t now_ms() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

/*
 * Generate a simple UUID-like string.
 * Not cryptographically secure, but good enough for correlation IDs.
 */
std::string generate_uuid();

}  // namespace utils

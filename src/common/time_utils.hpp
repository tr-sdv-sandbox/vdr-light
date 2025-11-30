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

/// @file time_utils.hpp
/// @brief Time utilities for VDR ecosystem

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

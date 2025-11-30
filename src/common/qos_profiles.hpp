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

/// @file qos_profiles.hpp
/// @brief Predefined QoS profiles for VDR ecosystem
///
/// Defines standard QoS configurations matching the specification.

#include "common/dds_wrapper.hpp"

namespace dds {
namespace qos_profiles {

/*
 * Reliable Critical - for events that must not be lost.
 *
 * - Reliability: RELIABLE
 * - Durability: TRANSIENT_LOCAL (survives writer restarts)
 * - History: KEEP_ALL
 */
Qos reliable_critical();

/*
 * Reliable Standard - for important data with bounded history.
 *
 * - Reliability: RELIABLE
 * - Durability: VOLATILE
 * - History: KEEP_LAST with configurable depth
 */
Qos reliable_standard(int32_t history_depth = 100);

/*
 * Best Effort - for high-frequency, loss-tolerant data.
 *
 * - Reliability: BEST_EFFORT
 * - Durability: VOLATILE
 * - History: KEEP_LAST(1) or configurable
 */
Qos best_effort(int32_t history_depth = 1);

}  // namespace qos_profiles
}  // namespace dds

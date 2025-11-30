#pragma once

/*
 * qos_profiles.hpp - Predefined QoS profiles for VDR ecosystem
 *
 * Defines standard QoS configurations matching the specification.
 */

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

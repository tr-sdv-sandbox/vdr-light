/*
 * qos_profiles.cpp - Predefined QoS profiles for VDR ecosystem
 */

#include "common/qos_profiles.hpp"

namespace dds {
namespace qos_profiles {

Qos reliable_critical() {
    Qos qos;
    qos.reliability_reliable(DDS_SECS(10))
       .durability_transient_local()
       .history_keep_all();
    return qos;
}

Qos reliable_standard(int32_t history_depth) {
    Qos qos;
    qos.reliability_reliable(DDS_SECS(1))
       .durability_volatile()
       .history_keep_last(history_depth);
    return qos;
}

Qos best_effort(int32_t history_depth) {
    Qos qos;
    qos.reliability_best_effort()
       .durability_volatile()
       .history_keep_last(history_depth);
    return qos;
}

}  // namespace qos_profiles
}  // namespace dds

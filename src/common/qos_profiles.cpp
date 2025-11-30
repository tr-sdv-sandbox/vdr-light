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

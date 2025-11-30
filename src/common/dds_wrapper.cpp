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

#include "common/dds_wrapper.hpp"

#include <glog/logging.h>

#include <sstream>
#include <utility>

namespace dds {

// Error implementation

Error::Error(dds_return_t code, std::string_view context)
    : std::runtime_error([&] {
          std::ostringstream oss;
          if (!context.empty()) {
              oss << context << ": ";
          }
          oss << "DDS error " << code << " (" << dds_strretcode(code) << ")";
          return oss.str();
      }()),
      code_(code) {}

// Entity implementation

Entity::Entity(dds_entity_t handle) : handle_(handle) {
    if (handle_ < 0) {
        throw Error(handle_, "Entity creation");
    }
}

Entity::~Entity() {
    if (handle_ > 0) {
        dds_return_t rc = dds_delete(handle_);
        if (rc != DDS_RETCODE_OK) {
            LOG(WARNING) << "Failed to delete DDS entity " << handle_ << ": "
                         << dds_strretcode(rc);
        }
    }
}

Entity::Entity(Entity&& other) noexcept : handle_(other.handle_) {
    other.handle_ = DDS_RETCODE_BAD_PARAMETER;
}

Entity& Entity::operator=(Entity&& other) noexcept {
    if (this != &other) {
        if (handle_ > 0) {
            dds_delete(handle_);
        }
        handle_ = other.handle_;
        other.handle_ = DDS_RETCODE_BAD_PARAMETER;
    }
    return *this;
}

dds_entity_t Entity::release() noexcept {
    dds_entity_t h = handle_;
    handle_ = DDS_RETCODE_BAD_PARAMETER;
    return h;
}

// Participant implementation

Participant::Participant(dds_domainid_t domain,
                         const dds_qos_t* qos,
                         const dds_listener_t* listener)
    : entity_(dds_create_participant(domain, qos, listener)) {
    LOG(INFO) << "Created DDS participant on domain " << domain;
}

// Topic implementation

Topic::Topic(const Participant& participant,
             const dds_topic_descriptor_t* descriptor,
             std::string_view name,
             const dds_qos_t* qos,
             const dds_listener_t* listener)
    : entity_(dds_create_topic(participant.get(),
                               descriptor,
                               std::string(name).c_str(),
                               qos,
                               listener)),
      name_(name) {
    LOG(INFO) << "Created DDS topic: " << name_;
}

// Writer implementation

Writer::Writer(const Participant& participant,
               const Topic& topic,
               const dds_qos_t* qos,
               const dds_listener_t* listener)
    : entity_(dds_create_writer(participant.get(), topic.get(), qos, listener)) {
    LOG(INFO) << "Created DDS writer for topic: " << topic.name();
}

// Reader implementation

Reader::Reader(const Participant& participant,
               const Topic& topic,
               const dds_qos_t* qos,
               const dds_listener_t* listener)
    : entity_(dds_create_reader(participant.get(), topic.get(), qos, listener)),
      waitset_(dds_create_waitset(participant.get())) {

    // Attach reader to waitset for blocking reads
    dds_return_t rc = dds_waitset_attach(waitset_.get(), entity_.get(), 0);
    if (rc != DDS_RETCODE_OK) {
        throw Error(rc, "dds_waitset_attach");
    }

    LOG(INFO) << "Created DDS reader for topic: " << topic.name();
}

bool Reader::wait(int32_t timeout_ms) {
    dds_attach_t triggered;
    dds_return_t rc = dds_waitset_wait(
        waitset_.get(),
        &triggered,
        1,
        DDS_MSECS(timeout_ms));

    if (rc < 0) {
        throw Error(rc, "dds_waitset_wait");
    }

    return rc > 0;  // Returns number of triggered conditions
}

// Qos implementation

Qos::Qos() : qos_(dds_create_qos()) {
    if (qos_ == nullptr) {
        throw std::runtime_error("Failed to create QoS");
    }
}

Qos::~Qos() {
    if (qos_ != nullptr) {
        dds_delete_qos(qos_);
    }
}

Qos::Qos(Qos&& other) noexcept : qos_(other.qos_) {
    other.qos_ = nullptr;
}

Qos& Qos::operator=(Qos&& other) noexcept {
    if (this != &other) {
        if (qos_ != nullptr) {
            dds_delete_qos(qos_);
        }
        qos_ = other.qos_;
        other.qos_ = nullptr;
    }
    return *this;
}

Qos& Qos::reliability_reliable(dds_duration_t max_blocking_time) {
    dds_qset_reliability(qos_, DDS_RELIABILITY_RELIABLE, max_blocking_time);
    return *this;
}

Qos& Qos::reliability_best_effort() {
    dds_qset_reliability(qos_, DDS_RELIABILITY_BEST_EFFORT, 0);
    return *this;
}

Qos& Qos::durability_volatile() {
    dds_qset_durability(qos_, DDS_DURABILITY_VOLATILE);
    return *this;
}

Qos& Qos::durability_transient_local() {
    dds_qset_durability(qos_, DDS_DURABILITY_TRANSIENT_LOCAL);
    return *this;
}

Qos& Qos::history_keep_last(int32_t depth) {
    dds_qset_history(qos_, DDS_HISTORY_KEEP_LAST, depth);
    return *this;
}

Qos& Qos::history_keep_all() {
    dds_qset_history(qos_, DDS_HISTORY_KEEP_ALL, 0);
    return *this;
}

}  // namespace dds

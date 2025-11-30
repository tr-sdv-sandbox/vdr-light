#pragma once

/*
 * dds_wrapper.hpp - RAII C++ wrappers for Cyclone DDS
 *
 * Provides type-safe, exception-safe wrappers around Cyclone DDS C API.
 * All DDS entities are automatically cleaned up on destruction.
 */

#include <dds/dds.h>

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace dds {

/*
 * Exception thrown for DDS errors.
 */
class Error : public std::runtime_error {
public:
    explicit Error(dds_return_t code, std::string_view context = "");
    dds_return_t code() const noexcept { return code_; }

private:
    dds_return_t code_;
};

/*
 * RAII wrapper for DDS entity handles.
 *
 * All DDS entities (participant, topic, reader, writer, etc.) are represented
 * as dds_entity_t in Cyclone. This wrapper ensures proper cleanup.
 */
class Entity {
public:
    Entity() noexcept : handle_(DDS_RETCODE_BAD_PARAMETER) {}
    explicit Entity(dds_entity_t handle);
    ~Entity();

    // Move semantics
    Entity(Entity&& other) noexcept;
    Entity& operator=(Entity&& other) noexcept;

    // No copying
    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;

    // Access underlying handle
    dds_entity_t get() const noexcept { return handle_; }
    dds_entity_t release() noexcept;

    // Validity check
    bool valid() const noexcept { return handle_ > 0; }
    explicit operator bool() const noexcept { return valid(); }

private:
    dds_entity_t handle_;
};

/*
 * DDS Participant - entry point to DDS domain.
 */
class Participant {
public:
    explicit Participant(dds_domainid_t domain = DDS_DOMAIN_DEFAULT,
                         const dds_qos_t* qos = nullptr,
                         const dds_listener_t* listener = nullptr);

    dds_entity_t get() const noexcept { return entity_.get(); }
    explicit operator bool() const noexcept { return entity_.valid(); }

private:
    Entity entity_;
};

/*
 * DDS Topic.
 */
class Topic {
public:
    Topic(const Participant& participant,
          const dds_topic_descriptor_t* descriptor,
          std::string_view name,
          const dds_qos_t* qos = nullptr,
          const dds_listener_t* listener = nullptr);

    dds_entity_t get() const noexcept { return entity_.get(); }
    const std::string& name() const noexcept { return name_; }
    explicit operator bool() const noexcept { return entity_.valid(); }

private:
    Entity entity_;
    std::string name_;
};

/*
 * DDS DataWriter.
 */
class Writer {
public:
    Writer(const Participant& participant,
           const Topic& topic,
           const dds_qos_t* qos = nullptr,
           const dds_listener_t* listener = nullptr);

    dds_entity_t get() const noexcept { return entity_.get(); }
    explicit operator bool() const noexcept { return entity_.valid(); }

    // Write a sample
    template<typename T>
    void write(const T& sample);

    // Write with timestamp
    template<typename T>
    void write(const T& sample, dds_time_t timestamp);

private:
    Entity entity_;
};

/*
 * DDS DataReader.
 */
class Reader {
public:
    Reader(const Participant& participant,
           const Topic& topic,
           const dds_qos_t* qos = nullptr,
           const dds_listener_t* listener = nullptr);

    dds_entity_t get() const noexcept { return entity_.get(); }
    explicit operator bool() const noexcept { return entity_.valid(); }

    // Take samples (removes from reader cache)
    // WARNING: Returned samples contain pointers to DDS-managed memory.
    // Process immediately before any other DDS operations.
    template<typename T>
    std::vector<T> take(size_t max_samples = 100);

    // Take and process each sample with a callback (recommended)
    // Callback is invoked while DDS loan is still valid - safe for string access
    template<typename T, typename Callback>
    size_t take_each(Callback&& callback, size_t max_samples = 100);

    // Read samples (leaves in reader cache)
    template<typename T>
    std::vector<T> read(size_t max_samples = 100);

    // Wait for data with timeout (milliseconds)
    bool wait(int32_t timeout_ms);

private:
    Entity entity_;
    Entity waitset_;
};

/*
 * RAII wrapper for QoS.
 */
class Qos {
public:
    Qos();
    ~Qos();

    Qos(Qos&& other) noexcept;
    Qos& operator=(Qos&& other) noexcept;

    Qos(const Qos&) = delete;
    Qos& operator=(const Qos&) = delete;

    dds_qos_t* get() noexcept { return qos_; }
    const dds_qos_t* get() const noexcept { return qos_; }

    // Fluent API for setting QoS policies
    Qos& reliability_reliable(dds_duration_t max_blocking_time = DDS_SECS(1));
    Qos& reliability_best_effort();
    Qos& durability_volatile();
    Qos& durability_transient_local();
    Qos& history_keep_last(int32_t depth);
    Qos& history_keep_all();

private:
    dds_qos_t* qos_;
};

// Template implementations

template<typename T>
void Writer::write(const T& sample) {
    dds_return_t rc = dds_write(entity_.get(), &sample);
    if (rc != DDS_RETCODE_OK) {
        throw Error(rc, "dds_write");
    }
}

template<typename T>
void Writer::write(const T& sample, dds_time_t timestamp) {
    dds_return_t rc = dds_write_ts(entity_.get(), &sample, timestamp);
    if (rc != DDS_RETCODE_OK) {
        throw Error(rc, "dds_write_ts");
    }
}

template<typename T>
std::vector<T> Reader::take(size_t max_samples) {
    std::vector<T> results;
    results.reserve(max_samples);

    std::vector<void*> samples(max_samples, nullptr);
    std::vector<dds_sample_info_t> infos(max_samples);

    dds_return_t count = dds_take(entity_.get(), samples.data(), infos.data(),
                                   max_samples, max_samples);

    if (count < 0) {
        throw Error(count, "dds_take");
    }

    for (int32_t i = 0; i < count; ++i) {
        if (infos[i].valid_data && samples[i] != nullptr) {
            results.push_back(*static_cast<T*>(samples[i]));
        }
    }

    // Return loan before returning - samples are shallow copies
    // Caller must be aware string pointers are only valid until next DDS operation
    dds_return_loan(entity_.get(), samples.data(), count);

    return results;
}

template<typename T, typename Callback>
size_t Reader::take_each(Callback&& callback, size_t max_samples) {
    std::vector<void*> samples(max_samples, nullptr);
    std::vector<dds_sample_info_t> infos(max_samples);

    dds_return_t count = dds_take(entity_.get(), samples.data(), infos.data(),
                                   max_samples, max_samples);

    if (count < 0) {
        throw Error(count, "dds_take");
    }

    size_t valid_count = 0;
    for (int32_t i = 0; i < count; ++i) {
        if (infos[i].valid_data && samples[i] != nullptr) {
            callback(*static_cast<T*>(samples[i]));
            ++valid_count;
        }
    }

    // Return loan after callback processing
    dds_return_loan(entity_.get(), samples.data(), count);

    return valid_count;
}

template<typename T>
std::vector<T> Reader::read(size_t max_samples) {
    std::vector<T> results;
    results.reserve(max_samples);

    std::vector<void*> samples(max_samples, nullptr);
    std::vector<dds_sample_info_t> infos(max_samples);

    dds_return_t count = dds_read(entity_.get(), samples.data(), infos.data(),
                                   max_samples, max_samples);

    if (count < 0) {
        throw Error(count, "dds_read");
    }

    for (int32_t i = 0; i < count; ++i) {
        if (infos[i].valid_data && samples[i] != nullptr) {
            results.push_back(*static_cast<T*>(samples[i]));
        }
    }

    // Return loan - samples are shallow copies
    dds_return_loan(entity_.get(), samples.data(), count);

    return results;
}

}  // namespace dds

# Kuksa-DDS-RT Integration Architecture

## Overview

This document describes the integration between:
- **Kuksa Databroker** - VSS-compliant API for in-vehicle clients
- **DDS** - Real-time pub/sub on HPC side
- **RT Arbiter** - Bare-metal real-time controller

The key principle: **RT side is the source of truth**. All actuator logic lives on RT. HPC side (Kuksa, DDS, Bridge) is transport and API layer.

## Data Flow

### Sensors (RT → Clients)

```
ECU/Sensors → RT → [Transport] → HPC DDS → Bridge → Kuksa → Apps
```

1. RT side reads sensors, publishes to HPC via transport layer
2. HPC DDS receives sensor data (via AVTP probe or similar)
3. Bridge subscribes to DDS, updates Kuksa actual values
4. Apps subscribe to Kuksa, receive updates

### Actuators (Clients → RT)

```
Apps → Kuksa → Bridge → HPC DDS → [Transport] → RT Arbiter → ECU
                                                     │
Apps ← Kuksa ← Bridge ← HPC DDS ← [Transport] ←──────┘ (actual value)
```

1. App calls `set("Vehicle.Cabin.Light.Intensity", 50)` on Kuksa
2. Kuksa routes to Bridge (registered as provider)
3. Bridge publishes target request to DDS
4. Transport layer forwards to RT Arbiter
5. Arbiter validates, arbitrates, commands ECU
6. Arbiter publishes actual value back
7. Transport delivers to HPC DDS
8. Bridge receives actual, updates Kuksa
9. App sees result via subscription

## Component Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              IN-VEHICLE CLIENTS                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
│  │ Android App │  │ HPC Service │  │ Infotainment│  │ Diagnostics │         │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘         │
│         │                │                │                │                │
│         └────────────────┴────────────────┴────────────────┘                │
│                                   │                                          │
│                                   │ gRPC (COVESA VSS API)                    │
│                                   ▼                                          │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                         KUKSA DATABROKER                               │  │
│  │  - VSS data model and API                                              │  │
│  │  - Subscription management                                             │  │
│  │  - Actuator provider routing                                           │  │
│  │  - Authorization                                                       │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                   │                                          │
│                                   │ Provider API                             │
│                                   ▼                                          │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                      KUKSA-DDS BRIDGE (generated)                      │  │
│  │                                                                        │  │
│  │  For each VSS actuator path:                                           │  │
│  │  - Register as provider in Kuksa                                       │  │
│  │  - on_target(value) → publish to DDS                                   │  │
│  │  - on_dds_actual(signal) → update Kuksa                                │  │
│  │                                                                        │  │
│  │  For each VSS sensor path:                                             │  │
│  │  - Subscribe to DDS                                                    │  │
│  │  - on_dds_signal(signal) → update Kuksa                                │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                   │                                          │
│                                   │ DDS pub/sub                              │
│                                   ▼                                          │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                            HPC DDS BUS                                 │  │
│  │                                                                        │  │
│  │  Topics:                                                               │  │
│  │    rt/vss/signals              - All VSS signals (sensors + actuals)   │  │
│  │    rt/vss/actuators/target     - Actuator target requests              │  │
│  │    rt/events/vehicle           - Vehicle events                        │  │
│  │    rt/telemetry/*              - Metrics, logs                         │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                   │                                          │
└───────────────────────────────────┼─────────────────────────────────────────┘
                                    │
                          HPC ──────┼────── RT
                                    │
                    ┌───────────────┴───────────────┐
                    │      TRANSPORT OPTIONS        │
                    │      (see section below)      │
                    └───────────────┬───────────────┘
                                    │
┌───────────────────────────────────┼─────────────────────────────────────────┐
│                                   ▼                                          │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                           RT ARBITER                                   │  │
│  │                                                                        │  │
│  │  Inputs (all treated equally):                                         │  │
│  │  - Sensor readings from ECUs/CAN                                       │  │
│  │  - Actuator target requests from HPC                                   │  │
│  │  - Internal requests from safety systems                               │  │
│  │                                                                        │  │
│  │  Logic:                                                                │  │
│  │  - Safety validation                                                   │  │
│  │  - Conflict resolution / priority arbitration                          │  │
│  │  - Rate limiting                                                       │  │
│  │  - State machine per actuator                                          │  │
│  │                                                                        │  │
│  │  Outputs:                                                              │  │
│  │  - Commands to ECUs                                                    │  │
│  │  - Actual values to HPC                                                │  │
│  │  - Rejection notifications                                             │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                   │                                          │
│                                   ▼                                          │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                          ECUs / ACTUATORS                              │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
│                              RT SIDE (bare metal)                            │
└─────────────────────────────────────────────────────────────────────────────┘
```

## HPC ↔ RT Transport Options

The transport between HPC (Linux/DDS) and RT (bare metal) needs:
- Deterministic delivery
- Low latency
- Simple protocol (bare metal has no full network stack)
- Bidirectional

### Option 1: IEEE 1722 AVTP (Both Directions)

Use the existing AVTP infrastructure for bidirectional communication.

```
HPC                                              RT
┌──────────────┐                            ┌──────────────┐
│  AVTP Probe  │◀───── AVTP CAN Frames ────▶│  AVTP Stack  │
│  (existing)  │      Stream ID: 0x01       │              │
│              │                            │              │
│  AVTP Sink   │◀───── AVTP CAN Frames ────▶│  AVTP Source │
│  (new)       │      Stream ID: 0x02       │  (new)       │
└──────────────┘                            └──────────────┘
```

**Pros:**
- Reuses existing infrastructure
- Standardized framing
- Time-synchronized (802.1AS)

**Cons:**
- CAN frame overhead for non-CAN data
- May need custom ACF type for actuator commands

**Message format (actuator target):**
```
AVTP Header (Stream ID for actuator targets)
└─ ACF Header (custom type?)
   └─ Payload:
      ├─ VSS path hash (4 bytes)
      ├─ Value type (1 byte)
      ├─ Value (8 bytes max)
      └─ Timestamp (8 bytes)
```

### Option 2: Raw Ethernet Frames

Custom ethertype, minimal parsing on RT side.

```
HPC                                              RT
┌──────────────┐                            ┌──────────────┐
│  Raw Socket  │◀───── EtherType 0x88xx ───▶│  ETH Driver  │
│  Publisher   │                            │  Parser      │
└──────────────┘                            └──────────────┘
```

**Frame format:**
```
Ethernet Header (14 bytes)
├─ Dest MAC: RT interface
├─ Src MAC: HPC interface
└─ EtherType: 0x88xx (custom)

Payload (fixed size for simplicity)
├─ Message type (1 byte): SENSOR=1, ACTUATOR_TARGET=2, ACTUATOR_ACTUAL=3
├─ VSS path hash (4 bytes)
├─ Sequence number (4 bytes)
├─ Timestamp (8 bytes)
├─ Value type (1 byte)
├─ Value (8 bytes)
└─ CRC (4 bytes)

Total: 14 + 30 = 44 bytes per message
```

**Pros:**
- Minimal overhead
- Trivial to parse on bare metal
- No IP stack required

**Cons:**
- Custom protocol to maintain
- No built-in reliability (add sequence numbers + retries if needed)

### Option 3: UDP (Minimal IP Stack)

If RT side has a minimal IP stack (lwIP or similar).

```
HPC                                              RT
┌──────────────┐                            ┌──────────────┐
│  UDP Socket  │◀─────── UDP Packets ──────▶│  lwIP/uIP   │
│  Port 5001   │                            │  Port 5001   │
└──────────────┘                            └──────────────┘
```

**Pros:**
- Standard tooling (Wireshark debugging)
- Can route through switches
- Checksum included

**Cons:**
- Requires IP stack on RT (memory/complexity)
- IP/UDP header overhead
- Non-deterministic without QoS

### Option 4: Shared Memory (Hypervisor Setup)

If HPC and RT run on same SoC with hypervisor.

```
┌─────────────────────────────────────────────────────────────┐
│                         SoC                                  │
│  ┌──────────────────┐         ┌──────────────────┐          │
│  │   HPC (Linux)    │         │   RT (bare metal)│          │
│  │                  │         │                  │          │
│  │  ┌────────────┐  │         │  ┌────────────┐  │          │
│  │  │ Ring Buffer│◀─┼────────▶│─▶│ Ring Buffer│  │          │
│  │  └────────────┘  │ Shared  │  └────────────┘  │          │
│  │                  │ Memory  │                  │          │
│  └──────────────────┘         └──────────────────┘          │
│                                                              │
│                      Hypervisor                              │
└─────────────────────────────────────────────────────────────┘
```

**Ring buffer structure:**
```c
struct shared_ring {
    volatile uint32_t head;        // Written by producer
    volatile uint32_t tail;        // Written by consumer
    uint32_t size;                 // Power of 2
    struct message buffer[SIZE];   // Fixed-size messages
};
```

**Pros:**
- Lowest latency
- No network overhead
- Deterministic

**Cons:**
- Only works with hypervisor setup
- Requires memory mapping coordination
- Cache coherency considerations

### Option 5: virtio/rpmsg (Hypervisor with Standard Interface)

Standard virtualization interface for inter-VM communication.

**Pros:**
- Standard interface
- Hypervisor handles details
- Existing drivers

**Cons:**
- Requires hypervisor support
- More complex than raw shared memory

## Recommendation

| Scenario | Recommended Option |
|----------|-------------------|
| Separate physical boards | Option 1 (AVTP) or Option 2 (Raw Ethernet) |
| Same SoC, hypervisor | Option 4 (Shared Memory) or Option 5 (virtio) |
| RT has IP stack | Option 3 (UDP) |
| Need time sync | Option 1 (AVTP with 802.1AS) |
| Simplest bare metal | Option 2 (Raw Ethernet) |

## Code Generation

The Kuksa-DDS Bridge should be generated from VSS specification to avoid boilerplate.

### Input: VSS Actuator Specification

```yaml
# actuators.yaml
actuators:
  - path: Vehicle.Cabin.Light.AmbientLight.Intensity
    type: uint8
    range: [0, 100]
    description: Ambient light intensity

  - path: Vehicle.Cabin.HVAC.Station.Row1.Left.Temperature
    type: float
    unit: celsius
    range: [-40, 85]
    description: Target temperature for left front seat

  - path: Vehicle.Chassis.Accelerator.PedalPosition
    type: uint8
    range: [0, 100]
    safety_critical: true
    description: Accelerator pedal position (read-only for most clients)
```

### Output: Generated Bridge Code

```cpp
// GENERATED FROM actuators.yaml - DO NOT EDIT

#include "actuator_proxy_base.hpp"

namespace vdr {
namespace kuksa_bridge {

class ActuatorProxy_Vehicle_Cabin_Light_AmbientLight_Intensity
    : public ActuatorProxyBase {
public:
    static constexpr const char* VSS_PATH =
        "Vehicle.Cabin.Light.AmbientLight.Intensity";

    void on_target_changed(const kuksa::Value& value) override {
        telemetry_vss_Signal msg{};
        msg.path = const_cast<char*>(VSS_PATH);
        msg.header.source_id = const_cast<char*>("kuksa_bridge");
        msg.header.timestamp_ns = utils::now_ns();
        msg.header.seq_num = seq_++;
        msg.value_type = telemetry_vss_VALUE_TYPE_UINT8;
        msg.uint8_value = value.get_uint8();

        dds_writer_->write(msg);
    }

    void on_dds_actual(const telemetry_vss_Signal& signal) override {
        kuksa_client_->set_actual(VSS_PATH, signal.uint8_value);
    }
};

// ... more actuator proxies ...

inline std::vector<std::unique_ptr<ActuatorProxyBase>>
create_all_actuator_proxies(
    KuksaClient& kuksa,
    dds::Writer& writer
) {
    std::vector<std::unique_ptr<ActuatorProxyBase>> proxies;

    proxies.push_back(
        std::make_unique<ActuatorProxy_Vehicle_Cabin_Light_AmbientLight_Intensity>()
    );
    proxies.push_back(
        std::make_unique<ActuatorProxy_Vehicle_Cabin_HVAC_Station_Row1_Left_Temperature>()
    );
    // ... all actuators ...

    return proxies;
}

}  // namespace kuksa_bridge
}  // namespace vdr
```

### Sensor Bridge (simpler)

Sensors are read-only in Kuksa, so the bridge just subscribes to DDS and updates Kuksa:

```cpp
// For sensors, no per-path class needed - just a generic subscriber
class SensorBridge {
public:
    void on_dds_signal(const telemetry_vss_Signal& signal) {
        // Update Kuksa with whatever came from DDS
        kuksa_client_->set(signal.path, to_kuksa_value(signal));
    }
};
```

## IDL Extensions

Add to `idl/telemetry.idl` for actuator request/response tracking:

```idl
module actuators {

    enum RequestStatus {
        REQUEST_PENDING,
        REQUEST_ACCEPTED,
        REQUEST_REJECTED,
        REQUEST_EXECUTING,
        REQUEST_COMPLETED,
        REQUEST_FAILED
    };

    @topic
    struct ActuatorRequest {
        telemetry::Header header;
        string path;                    // VSS path
        vss::Signal target;             // Desired value
        string request_id;              // UUID for tracking
    };

    @topic
    struct ActuatorResponse {
        telemetry::Header header;
        string request_id;              // Matches request
        RequestStatus status;
        string reason;                  // If rejected/failed
        vss::Signal actual;             // Current actual value
    };

};
```

## Open Questions

1. **Actuator rejection feedback** - How should apps be notified when RT arbiter rejects a request? Options:
   - Kuksa error response (requires extending provider API)
   - Separate notification channel
   - Just don't update actual value (app infers from no change)

2. **Request timeout** - If RT doesn't respond, when does the bridge give up?

3. **Actuator state machine** - Should HPC track actuator states (idle, pending, executing) or is that purely RT concern?

4. **Batching** - Should multiple actuator requests be batched in single transport message?

5. **Priority** - Should some actuator requests (safety-critical) have priority in transport?

## Next Steps

1. Decide on HPC ↔ RT transport option
2. Define message format for chosen transport
3. Implement RT-side message handler
4. Implement HPC-side transport bridge
5. Create code generator for Kuksa-DDS bridge
6. Integration testing with real Kuksa databroker

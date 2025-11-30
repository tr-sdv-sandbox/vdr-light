#!/bin/bash
#
# run_all.sh - Start all VDR ecosystem services and tear down on exit
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
CONFIG_FILE="${SCRIPT_DIR}/config/vdr_config.yaml"

# PIDs of child processes
declare -a PIDS=()

# Cleanup function - kills all child processes
cleanup() {
    echo ""
    echo "[INFO] Shutting down all services..."

    for pid in "${PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill -TERM "$pid" 2>/dev/null || true
        fi
    done

    sleep 1

    for pid in "${PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill -9 "$pid" 2>/dev/null || true
        fi
    done

    echo "[INFO] All services stopped."
    exit 0
}

trap cleanup SIGINT SIGTERM EXIT

# Check if executables exist
if [[ ! -x "${BUILD_DIR}/vdr" ]]; then
    echo "[ERROR] Executables not found. Build first with:"
    echo "  cmake -B build && cmake --build build"
    exit 1
fi

echo ""
echo "=== VDR Ecosystem ==="
echo ""

# Start VDR
echo "[INFO] Starting VDR..."
"${BUILD_DIR}/vdr" "${CONFIG_FILE}" &
PIDS+=($!)
sleep 0.5

# Start VSS Probe
echo "[INFO] Starting VSS Probe..."
"${BUILD_DIR}/probe_vss" 10 &
PIDS+=($!)

# Start Metrics Probe
echo "[INFO] Starting Metrics Probe..."
"${BUILD_DIR}/probe_metrics" 5 &
PIDS+=($!)

# Start Event Probe
echo "[INFO] Starting Event Probe..."
"${BUILD_DIR}/probe_events" 2 &
PIDS+=($!)

echo ""
echo "[INFO] All services started. Press Ctrl+C to stop."
echo ""

# Wait forever
wait

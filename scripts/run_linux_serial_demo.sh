#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

mkdir -p build run_logs
bash scripts/build_native.sh

cleanup() {
    jobs -p | xargs -r kill 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "Starting mock STM32 serial device..."
./build/mock_stm32 --link /tmp/self_reconfig_stm32 --battery 92 \
    > run_logs/mock_stm32.log 2> run_logs/mock_stm32.err &

sleep 1
ls -l /tmp/self_reconfig_stm32

echo "Starting master_node..."
./build/master_node --state-file build/state.json \
    > run_logs/master_node.log 2> run_logs/master_node.err &

sleep 1

echo "Starting stm32_bridge..."
./build/stm32_bridge \
    --id 1 \
    --master 127.0.0.1 \
    --port 9000 \
    --serial /tmp/self_reconfig_stm32 \
    --baud 115200 \
    --x 0 \
    --y 0 \
    --battery 92 \
    --drive-pwm 220 \
    > run_logs/stm32_bridge.log 2> run_logs/stm32_bridge.err &

echo
echo "Linux serial demo is running for 20 seconds."
echo "Logs:"
echo "  run_logs/mock_stm32.log"
echo "  run_logs/master_node.log"
echo "  run_logs/stm32_bridge.log"
echo

sleep 20

echo "===== stm32 bridge log ====="
tail -n 30 run_logs/stm32_bridge.log || true
echo
echo "===== mock stm32 log ====="
tail -n 30 run_logs/mock_stm32.log || true

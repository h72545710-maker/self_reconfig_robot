#!/usr/bin/env bash
set -euo pipefail

units=(
  self-reconfig-ros2-bridge.service
  self-reconfig-robot-sim.service
  self-reconfig-mock-stm32.service
  self-reconfig-health.timer
  self-reconfig-health.service
)

for unit in "${units[@]}"; do
    sudo systemctl disable --now "$unit" 2>/dev/null || true
done

for unit in "${units[@]}"; do
    sudo rm -f "/etc/systemd/system/${unit}"
done

sudo systemctl daemon-reload
echo "Removed self-reconfig systemd units."

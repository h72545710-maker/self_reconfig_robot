#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="${PROJECT_DIR:-$HOME/self_reconfig_robot}"
SERVICE_USER="${SERVICE_USER:-$USER}"
SYSTEMD_DIR="${SYSTEMD_DIR:-/etc/systemd/system}"

cd "$(dirname "$0")/.."

if [[ ! -d deploy/systemd ]]; then
    echo "deploy/systemd not found" >&2
    exit 1
fi

echo "Installing systemd units"
echo "  project: ${PROJECT_DIR}"
echo "  user:    ${SERVICE_USER}"

bash scripts/build_native.sh
chmod +x scripts/*.sh

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

for unit in deploy/systemd/self-reconfig-*; do
    name="$(basename "$unit")"
    sed \
        -e "s#/home/yu/self_reconfig_robot#${PROJECT_DIR}#g" \
        -e "s#User=yu#User=${SERVICE_USER}#g" \
        "$unit" > "${tmp_dir}/${name}"
    sudo install -m 0644 "${tmp_dir}/${name}" "${SYSTEMD_DIR}/${name}"
done

sudo systemctl daemon-reload
sudo systemctl enable self-reconfig-robot-sim.service
sudo systemctl enable self-reconfig-health.timer

echo
echo "Installed. Useful commands:"
echo "  sudo systemctl start self-reconfig-robot-sim.service"
echo "  sudo systemctl start self-reconfig-health.timer"
echo "  sudo systemctl status self-reconfig-robot-sim.service"
echo "  journalctl -u self-reconfig-robot-sim.service -f"
echo
echo "Optional after ROS2 bridge is built:"
echo "  sudo systemctl enable --now self-reconfig-ros2-bridge.service"

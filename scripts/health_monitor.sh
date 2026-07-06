#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="${PROJECT_DIR:-$HOME/self_reconfig_robot}"
STATE_FILE="${STATE_FILE:-${PROJECT_DIR}/build/control_state.json}"
MAX_STATE_AGE_SEC="${MAX_STATE_AGE_SEC:-5}"

status=0

check_process() {
    local name="$1"
    local pattern="$2"
    if pgrep -f "$pattern" >/dev/null 2>&1; then
        echo "OK   process ${name} is running"
    else
        echo "WARN process ${name} is not running"
        status=1
    fi
}

check_state_file() {
    if [[ ! -f "$STATE_FILE" ]]; then
        echo "WARN state file missing: $STATE_FILE"
        status=1
        return
    fi

    local now
    local modified
    now="$(date +%s)"
    modified="$(stat -c %Y "$STATE_FILE")"
    local age=$((now - modified))
    if (( age <= MAX_STATE_AGE_SEC )); then
        echo "OK   state file fresh: ${STATE_FILE} age=${age}s"
    else
        echo "WARN state file stale: ${STATE_FILE} age=${age}s"
        status=1
    fi
}

check_serial_link() {
    local device="${SERIAL_DEVICE:-/tmp/self_reconfig_stm32}"
    if [[ -e "$device" ]]; then
        echo "OK   serial device exists: $device"
    else
        echo "INFO serial device not found: $device"
    fi
}

echo "self_reconfig health check"
echo "project=${PROJECT_DIR}"
echo

check_process "robot_sim" "build/robot_sim"
check_state_file
check_serial_link

echo
if (( status == 0 )); then
    echo "health=ok"
else
    echo "health=degraded"
fi

exit "$status"

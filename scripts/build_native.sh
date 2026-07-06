#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

mkdir -p build

CXX="${CXX:-g++}"
CXXFLAGS=(-std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude)

"$CXX" "${CXXFLAGS[@]}" \
  src/main.cpp \
  src/grid_map.cpp \
  src/sensor_fusion.cpp \
  -o build/robot_sim

"$CXX" "${CXXFLAGS[@]}" \
  src/master_node.cpp \
  src/protocol.cpp \
  src/udp_socket.cpp \
  -o build/master_node

"$CXX" "${CXXFLAGS[@]}" \
  src/module_node.cpp \
  src/protocol.cpp \
  src/udp_socket.cpp \
  -o build/module_node

"$CXX" "${CXXFLAGS[@]}" \
  src/stm32_bridge.cpp \
  src/protocol.cpp \
  src/serial_port.cpp \
  src/udp_socket.cpp \
  -o build/stm32_bridge

echo "native build complete"

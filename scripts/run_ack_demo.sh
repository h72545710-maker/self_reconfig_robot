#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

duration="${1:-6}"
module_duration=$((duration - 1))
if [ "$module_duration" -lt 1 ]; then
  module_duration=1
fi

mkdir -p run_logs
rm -f run_logs/*.log run_logs/*.err

bash scripts/build_native.sh

./build/master_node --duration "$duration" > run_logs/ack_master.log 2> run_logs/ack_master.err &
master_pid=$!

sleep 0.3

./build/module_node --id 1 --battery 95 --x 1 --y 1 --duration "$module_duration" --drop-ack-once 1 > run_logs/ack_module1.log 2> run_logs/ack_module1.err &
module1_pid=$!
./build/module_node --id 2 --battery 88 --x 1 --y 2 --duration "$module_duration" > run_logs/ack_module2.log 2> run_logs/ack_module2.err &
module2_pid=$!

wait "$master_pid" "$module1_pid" "$module2_pid"

echo
echo "===== master output ====="
cat run_logs/ack_master.log

echo
echo "===== module output ====="
cat run_logs/ack_module1.log
cat run_logs/ack_module2.log

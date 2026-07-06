#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

duration="${1:-8}"
module_duration=$((duration - 2))
if [ "$module_duration" -lt 1 ]; then
  module_duration=1
fi

mkdir -p run_logs
rm -f run_logs/*.log run_logs/*.err

bash scripts/build_native.sh

./build/master_node --duration "$duration" > run_logs/master.log 2> run_logs/master.err &
master_pid=$!

sleep 0.3

./build/module_node --id 1 --battery 95 --x 1 --y 1 --duration "$module_duration" --fail-after 4 > run_logs/module1.log 2> run_logs/module1.err &
module1_pid=$!
./build/module_node --id 2 --battery 88 --x 1 --y 2 --duration "$module_duration" > run_logs/module2.log 2> run_logs/module2.err &
module2_pid=$!
./build/module_node --id 3 --battery 80 --x 1 --y 3 --duration "$module_duration" > run_logs/module3.log 2> run_logs/module3.err &
module3_pid=$!

wait "$master_pid" "$module1_pid" "$module2_pid" "$module3_pid"

echo
echo "===== master output ====="
cat run_logs/master.log

echo
echo "===== module output ====="
cat run_logs/module1.log
cat run_logs/module2.log
cat run_logs/module3.log

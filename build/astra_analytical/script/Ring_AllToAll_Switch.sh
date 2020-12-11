#!/bin/bash
set -e

workload=medium_DLRM

# Absolue path to this script
SCRIPT_DIR=$(dirname "$(realpath $0)")

# Absolute paths to useful directories
PROJECT_DIR="${SCRIPT_DIR:?}"/../../..
INPUT_DIR=${PROJECT_DIR}/inputs
COMPILE_SCRIPT="${SCRIPT_DIR:?}"/../build.sh
BINARY="${SCRIPT_DIR:?}"/../build/AnalyticalAstra/bin/AnalyticalAstra
NETWORK=${INPUT_DIR}/network/analytical/sample_Ring_AllToAll_Switch.json
SYSTEM=${INPUT_DIR}/system/sample_3dim_sys
WORKLOAD=${INPUT_DIR}/workload/"$workload"
STATS="${SCRIPT_DIR:?}"/../result/ring-alltoall-switch-${1:-result}

# create result directory
rm -rf "${STATS}"
mkdir -p "${STATS}"

# compile binary
echo "[SCRIPT] Compiling AnalyticalAstra"
"${COMPILE_SCRIPT}" -c

nodes=(2 4 8 16 32 64 128)
commScale=(1)

current_row=-1
tot_stat_row=$((${#nodes[@]} * ${#commScale[@]}))

# run test
for i in "${!nodes[@]}"; do
  for inj in "${commScale[@]}"; do
    current_row=$(($current_row + 1))
    totalNPUs=$((${nodes[$i]} * 16))
    filename="workload-$workload-npus-$totalNPUs-commScale-$inj"

    echo "[SCRIPT] Initiate ${filename}"

    nohup "${BINARY}" \
      --network-configuration="${NETWORK}" \
      --system-configuration="${SYSTEM}" \
      --workload-configuration="${WORKLOAD}" \
      --path="${STATS}/" \
      --units-count 2 8 "${nodes[$i]}" \
      --num-passes 2 \
      --num-queues-per-dim 1 \
      --comm-scale $inj \
      --compute-scale 1 \
      --injection-scale 1 \
      --rendezvous-protocol false \
      --total-stat-rows "$tot_stat_row" \
      --stat-row "$current_row" \
      --run-name $filename >> "${STATS}/$filename.txt" &
      
      sleep 1
  done
done

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
NETWORK=${INPUT_DIR}/network/analytical/sample_Switch.json
SYSTEM=${INPUT_DIR}/system/sample_a2a_sys
WORKLOAD=${INPUT_DIR}/workload/"$workload"
STATS="${SCRIPT_DIR:?}"/../result/switch-${1:-result}

# create result directory
rm -rf "${STATS}"
mkdir -p "${STATS}"

# compile binary
echo "[SCRIPT] Compiling AnalyticalAstra"
"${COMPILE_SCRIPT}" -c

npus=(4 16 32 64 128 256 512 1024 2048)
commScale=(1)

current_row=-1
tot_stat_row=$((${#npus[@]} * ${#commScale[@]}))

# run test
for npu in "${npus[@]}"; do
  for inj in "${commScale[@]}"; do
    current_row=$(($current_row + 1))
    filename="workload-$workload-npus-${npu}-commScale-$inj"

    echo "[SCRIPT] Initiate ${filename}"

    nohup "${BINARY}" \
      --network-configuration="${NETWORK}" \
      --system-configuration="${SYSTEM}" \
      --workload-configuration="${WORKLOAD}" \
      --path="${STATS}/" \
      --units-count "${npu}" \
      --num-passes 2 \
      --num-queues-per-dim 1 \
      --comm-scale ${inj} \
      --compute-scale 1 \
      --injection-scale 1 \
      --rendezvous-protocol false \
      --total-stat-rows "${tot_stat_row}" \
      --stat-row "${current_row}" \
      --run-name ${filename} >> "${STATS}/${filename}.txt" &
      
      sleep 1
  done
done

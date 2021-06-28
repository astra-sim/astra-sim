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
NETWORK=${INPUT_DIR}/network/analytical/sample_Torus2D.json
SYSTEM=${INPUT_DIR}/system/sample_torus_sys
WORKLOAD=${INPUT_DIR}/workload/"$workload"
STATS="${SCRIPT_DIR:?}"/../result/torus-${1:-result}

# create result directory
rm -rf "${STATS}"
mkdir -p "${STATS}"

# compile binary
echo "[SCRIPT] Compiling AnalyticalAstra"
"${COMPILE_SCRIPT}" -c

# test sets
torus_width=(2 4 6 8 9 10 12 16 20 30 40)
commScale=(1)

current_row=-1
tot_stat_row=$((${#torus_width[@]} * ${#commScale[@]}))

# run test
for i in "${!torus_width[@]}"; do
  for inj in "${commScale[@]}"; do
    current_row=$(($current_row + 1))
    npus_count=$((${torus_width[$i]} * ${torus_width[$i]}))
    filename="workload-$workload-npus-${npus_count}-commScale-$inj"

    echo "[SCRIPT] Initiate ${filename}"

    nohup "${BINARY}" \
      --network-configuration="${NETWORK}" \
      --system-configuration="${SYSTEM}" \
      --workload-configuration="${WORKLOAD}" \
      --path="${STATS}/" \
      --units-count "${torus_width[$i]}" "${torus_width[$i]}" \
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

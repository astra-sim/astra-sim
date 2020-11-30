#! /bin/bash -v

# Absolue path to this script
SCRIPT_DIR=$(dirname "$(realpath $0)")

# Absolute paths to useful directories
BINARY="${SCRIPT_DIR:?}"/../build/astra_analytical/build/AnalyticalAstra/bin/AnalyticalAstra
NETWORK="${SCRIPT_DIR:?}"/../inputs/network/analytical/sample_switch.json
SYSTEM="${SCRIPT_DIR:?}"/../inputs/system/sample_a2a_sys.txt
WORKLOAD="${SCRIPT_DIR:?}"/../inputs/workload/microAllReduce.txt
STATS="${SCRIPT_DIR:?}"/results/run_allreduce_analytical

rm -rf "${STATS}"
mkdir "${STATS}"

"${BINARY}" \
--network-configuration="${NETWORK}" \
--system-configuration="${SYSTEM}" \
--workload-configuration="${WORKLOAD}" \
--path="${STATS}/" \
--run-name="sample_all_reduce" \
--num-passes=2 \
--total-stat-rows=1 \
--stat-row=0 


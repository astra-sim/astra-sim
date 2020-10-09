#! /bin/bash -v

workload=medium_DLRM

# Absolue path to this script
SCRIPT_DIR=$(dirname "$(realpath $0)")

# Absolute paths to useful directories
BINARY="${SCRIPT_DIR:?}"/build/AnalyticalAstra/bin/AnalyticalAstra
NETWORK="${SCRIPT_DIR:?}"/../../inputs/network/analytical/sample_torus.json
SYSTEM="${SCRIPT_DIR:?}"/../../inputs/system/sample_3dim_sys
WORKLOAD="${SCRIPT_DIR:?}"/../../inputs/workload/"$workload"
STATS="${SCRIPT_DIR:?}"/result/$1-3dim

rm -rf "${STATS}"
mkdir "${STATS}"

nodes=(2 4 8 16 32 64 128)
commScale=(1)

current_row=-1
tot_stat_row=`expr ${#nodes[@]} \* ${#commScale[@]}`


for i in "${!nodes[@]}"; do
  for inj in "${commScale[@]}"; do
    current_row=`expr $current_row + 1`
    totalNPUs=`expr ${nodes[$i]} \* 16`
    filename="workload-$workload-npus-$totalNPUs-commScale-$inj"
	"${BINARY}" \
	--network-configuration="${NETWORK}" \
	--system-configuration="${SYSTEM}" \
	--workload-configuration="${WORKLOAD}" \
	--path="${STATS}/" \
    --topology-name Ring_AllToAll_Switch \
    --dims-count 3 \
    --nodes-per-dim 2 8 "${nodes[$i]}" \
    --link-bandwidth 250 25 12.5 \
    --link-latency 500 500 500 \
    --nic-latency 0 0 1 \
    --router-latency 0 0 1 \
    --hbm-bandwidth 370 370 13 \
    --hbm-latency 500 500 500 \
    --hbm-scale 1 \
    --num-passes 2 \
    --num-queues-per-dim 1 \
    --comm-scale $inj \
    --compute-scale 1 \
    --injection-scale 1 \
    --rendezvous-protocol false \
    --total-stat-rows "$tot_stat_row" \
    --stat-row "$current_row" \
    --run-name $filename>>"${STATS}/$filename.txt"
    sleep 1
  done
done

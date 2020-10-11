#! /bin/bash -v

workload=DLRM_HybridParallel

# Absolue path to this script
SCRIPT_DIR=$(dirname "$(realpath $0)")

# Absolute paths to useful directories
BINARY="${SCRIPT_DIR:?}"/build/analytical/bin/AnalyticalAstra
NETWORK="${SCRIPT_DIR:?}"/../../inputs/network/analytical/sample_torus.json
SYSTEM="${SCRIPT_DIR:?}"/../../inputs/system/sample_a2a_sys
WORKLOAD="${SCRIPT_DIR:?}"/../../inputs/workload/"$workload"
STATS="${SCRIPT_DIR:?}"/result/$1-switch

rm -rf "${STATS}"
mkdir "${STATS}"

npus=(4) # 16 32 64 128 256 512 1024 2048)
commScale=(1)

current_row=-1
tot_stat_row=`expr ${#npus[@]} \* ${#commScale[@]}`


for i in "${!npus[@]}"; do
  for inj in "${commScale[@]}"; do
    current_row=`expr $current_row + 1`
    filename="workload-$workload-npus-${npus[$i]}-commScale-$inj"
	"${BINARY}" \
	--network-configuration="${NETWORK}" \
	--system-configuration="${SYSTEM}" \
	--workload-configuration="${WORKLOAD}" \
	--path="${STATS}/" \
    --topology-name switch \
    --dims-count 1 \
    --nodes-per-dim "${npus[$i]}" \
    --link-bandwidth 25 \
    --link-latency 500 \
    --nic-latency 0 \
    --router-latency 0 \
    --hbm-bandwidth 13 \
    --hbm-latency 500 \
    --hbm-scale 1 \
    --num-passes 2 \
    --num-queues-per-dim 1 \
    --comm-scale $inj \
    --compute-scale 1 \
    --injection-scale 1 \
    --rendezvous-protocol false \
    --total-stat-rows "$tot_stat_row" \
    --stat-row "$current_row" \
    --run-name $filename  # >>"${STATS}/$filename.txt"
    sleep 1
  done
done

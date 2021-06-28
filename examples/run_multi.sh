#! /bin/bash -v

# Absolue path to this script
SCRIPT_DIR=$(dirname "$(realpath $0)")

# Absolute paths to useful directories
BINARY="${SCRIPT_DIR:?}"/../build/astra_analytical/build/AnalyticalAstra/bin/AnalyticalAstra
NETWORK="${SCRIPT_DIR:?}"/../inputs/network/analytical/sample_Torus3D.json
SYSTEM="${SCRIPT_DIR:?}"/../inputs/system/sample_torus_sys.txt
WORKLOAD="${SCRIPT_DIR:?}"/../inputs/workload/microAllReduce.txt
STATS="${SCRIPT_DIR:?}"/results/run_multi

rm -rf "${STATS}"
mkdir "${STATS}"

npus=(2 4 6 8 9)
commScale=(1 2)
current_row=-1
tot_stat_row=`expr ${#npus[@]} \* ${#commScale[@]}`


for i in "${!npus[@]}"; do
  for inj in "${commScale[@]}"; do
    current_row=`expr $current_row + 1`
    tot_npus=`expr ${npus[$i]} \* ${npus[$i]} \* ${npus[$i]} `
    filename="npus-$tot_npus-commScale-$inj"
	"${BINARY}" \
    --network-configuration="${NETWORK}" \
    --system-configuration="${SYSTEM}" \
    --workload-configuration="${WORKLOAD}" \
    --path="${STATS}/" \
    --units-count "${npus[$i]}" "${npus[$i]}" "${npus[$i]}" \
    --links-count 2 2 2 \
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

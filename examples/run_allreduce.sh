#! /bin/bash -v

# Absolue path to this script
SCRIPT_DIR=$(dirname "$(realpath $0)")

# Absolute paths to useful directories
BINARY=""
NETWORK=""
SYSTEM="${SCRIPT_DIR:?}"/../inputs/system/sample_torus_sys
WORKLOAD="${SCRIPT_DIR:?}"/../inputs/workload/microAllReduce
STATS="${SCRIPT_DIR:?}"/results/run_allreduce

while getopts n: flag
do
    case "${flag}" in
        n) network=${OPTARG};;
    esac
done

echo "network: $network";

if [ "$network" == "gem5" ]
then
	BINARY=""
	NETWORK=""    
elif [ "$network" == "analytical" ]
then
    BINARY="${SCRIPT_DIR:?}"/../build/astra_analytical/build/AnalyticalAstra/bin/AnalyticalAstra
	NETWORK="${SCRIPT_DIR:?}"/../inputs/network/analytical/sample_torus.json
fi

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


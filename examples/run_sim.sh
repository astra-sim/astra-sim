#! /bin/bash -v

# Absolue path to this script
SCRIPT_DIR=$(dirname "$(realpath $0)")

# Absolute paths to useful directories
BINARY=""
NETWORK=""
CONFIG=""
SYNTHETIC=""
SYSTEM=""
WORKLOAD=""
STATS="${SCRIPT_DIR:?}"/results/run_sim

while getopts n:s:w: flag
do
    case "${flag}" in
        n) network=${OPTARG};;
		s) system=${OPTARG};;
		w) workload=${OPTARG};;
    esac
done

echo "network: $network"
echo "system: $system"
echo "workload: $workload"

if [ "$network" == "garnet" ]
then
	BINARY="${SCRIPT_DIR:?}"/../build/astra_garnet/build/gem5.opt
	NETWORK="${SCRIPT_DIR:?}"/../inputs/network/garnet/sample_torus
    SYNTHETIC="--synthetic=training"
    CONFIG="${SCRIPT_DIR:?}"/../extern/network_backend/garnet/gem5_astra/configs/example/garnet_synth_traffic.py    
elif [ "$network" == "analytical" ]
then
    BINARY="${SCRIPT_DIR:?}"/../build/astra_analytical/build/AnalyticalAstra/bin/AnalyticalAstra
	NETWORK="${SCRIPT_DIR:?}"/../inputs/network/analytical/sample_Torus2D.json
fi

SYSTEM="${SCRIPT_DIR:?}"/../inputs/system/$system
WORKLOAD="${SCRIPT_DIR:?}"/../inputs/workload/$workload


rm -rf "${STATS}"
mkdir "${STATS}"

"${BINARY}" "${CONFIG}" "$SYNTHETIC" \
--network-configuration="${NETWORK}" \
--system-configuration="${SYSTEM}" \
--workload-configuration="${WORKLOAD}" \
--path="${STATS}/" \
--run-name="sample_sim" \
--num-passes=2 \
--total-stat-rows=1 \
--stat-row=0 


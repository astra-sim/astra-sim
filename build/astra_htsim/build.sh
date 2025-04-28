#!/bin/bash
set -e
set -x

# set paths
SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR="${SCRIPT_DIR:?}/../.."
BUILD_DIR="${SCRIPT_DIR:?}"/build
CHAKRA_ET_DIR="${SCRIPT_DIR:?}"/../../extern/graph_frontend/chakra/schema/protobuf

# Absolute paths to useful directories
ASTRA_SIM_DIR="${SCRIPT_DIR:?}"/../../astra-sim
NS3_DIR="${SCRIPT_DIR:?}"/../../extern/network_backend/ns-3
# Inputs - change as necessary.
WORKLOAD="${SCRIPT_DIR:?}"/../../examples/network_analytical/workload/AllReduce_1MB
SYSTEM="${SCRIPT_DIR:?}"/../../examples/network_analytical/system.json
MEMORY="${SCRIPT_DIR:?}"/../../examples/network_analytical/remote_memory.json
NETWORK="${SCRIPT_DIR:?}"/../../examples/network_analytical/network.yml

TOPO="${SCRIPT_DIR:?}"/../../examples/htsim/8nodes.topo

# set functions
function compile_chakra_et() {
  # compile et_def.proto if one doesn't exist
  if [[ ! -f "${CHAKRA_ET_DIR:?}"/et_def.pb.h || ! -f "${CHAKRA_ET_DIR:?}"/et_def.pb.cc ]]; then
    protoc et_def.proto \
      --proto_path="${CHAKRA_ET_DIR:?}" \
      --cpp_out="${CHAKRA_ET_DIR:?}"
  fi

  if [[ ! -f "${CHAKRA_ET_DIR:?}"/et_def_pb2.py ]]; then
    protoc et_def.proto \
      --proto_path="${CHAKRA_ET_DIR:?}" \
      --python_out="${CHAKRA_ET_DIR:?}"
  fi
}

function setup() {
  # make build directory if one doesn't exist
  if [[ ! -d "${BUILD_DIR:?}" ]]; then
    mkdir -p "${BUILD_DIR:?}"
  fi

  # set concurrent build threads, capped at 16
  NUM_THREADS=$(nproc)
  if [[ ${NUM_THREADS} -ge 16 ]]; then
    NUM_THREADS=16
  fi
}

function patch_htsim() {
  patch -p1 -d "${SCRIPT_DIR:?}"/../../extern/network_backend/csg-htsim \
    --forward --reject-file=- -i "${SCRIPT_DIR:?}"/htsim_astrasim.patch && true
  ret=$?
  if [[ $ret -eq 0 ]]; then
    echo "HTSim patch applied successfully"
  elif [[ $ret -eq 1 ]]; then
    echo "HTSim patch skipped"
  else
    echo "HTSim patch failed"
    exit 1
  fi
}

function compile_astrasim_htsim() {
  # compile AstraSim
  cd "${BUILD_DIR:?}" || exit
  cmake ..
  cmake --build . -j "${NUM_THREADS:?}"
}

function compile_astrasim_htsim_as_debug() {
  # compile AstraSim
  cd "${BUILD_DIR:?}" || exit
  cmake .. -DCMAKE_BUILD_TYPE=Debug
  cmake --build . --config=Debug -j "${NUM_THREADS:?}"
}

function run() {
  cd "${BUILD_DIR:?}" || exit
  gdb --args ${PROJECT_DIR:?}/build/astra_htsim/build/bin/AstraSim_HTSim \
    --workload-configuration="${WORKLOAD}" \
    --system-configuration="${SYSTEM}" \
    --remote-memory-configuration="${MEMORY}" \
    --network-configuration="${NETWORK}" \
    --htsim_opts -topo ${TOPO}
}

function cleanup() {
  rm -rf "${BUILD_DIR:?}"
  rm -f "${CHAKRA_ET_DIR}/et_def.pb.cc"
  rm -f "${CHAKRA_ET_DIR}/et_def.pb.h"
  rm -f "${CHAKRA_ET_DIR}/et_def_pb2.py"
}

# set default option values
build_as_debug=false
should_clean=false
should_run=false

# Process command-line options
while getopts "ldr" OPT; do
  case "${OPT:?}" in
  l)
    should_clean=true
    ;;
  d)
    build_as_debug=true
    ;;
  r)
    should_run=true
    ;;
  *)
    exit 1
    ;;
  esac
done

# run operations as required
if [[ ${should_clean:?} == true ]]; then
  cleanup
else
  # setup ASTRA-sim build
  setup
  compile_chakra_et
  patch_htsim

  # compile ASTRA-sim
  if [[ ${build_as_debug:?} == true ]]; then
    compile_astrasim_htsim_as_debug
  else
    compile_astrasim_htsim
  fi

  if [[ ${should_run:?} == true ]]; then
    run
  fi
fi

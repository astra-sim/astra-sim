#!/bin/bash
set -e

# set paths
SCRIPT_DIR=$(dirname "$(realpath "$0")")
BUILD_DIR="${SCRIPT_DIR:?}"/build/

# set functions
function setup {
  # compile et_def.proto
  protoc et_def.proto \
    --proto_path "${SCRIPT_DIR:?}"/../../extern/graph_frontend/chakra/et_def/ \
    --cpp_out "${SCRIPT_DIR:?}"/../../extern/graph_frontend/chakra/et_def/

  # make build directory
  mkdir -p "${BUILD_DIR:?}"

  # set concurrent threads, capped at 16
  NUM_THREADS=$(nproc)
  if [[ ${NUM_THREADS} -ge 16 ]]; then
    NUM_THREADS=16
  fi
}

function compile {
  # compile AstraSim
  cd "${BUILD_DIR:?}" || exit
  cmake ..
  cmake --build . -j "${NUM_THREADS:?}"
}

function cleanup {
  rm -rf "${BUILD_DIR:?}"
}

case "$1" in
-l|--clean)
  cleanup;;
*)
  setup
  compile;;
esac

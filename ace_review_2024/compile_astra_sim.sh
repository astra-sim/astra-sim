#!/bin/bash
set -e

# Get paths
SCRIPT_DIR=$(dirname "$(realpath "$0")")
ASTRASIM_DIR=${SCRIPT_DIR:?}/..

# Build ASTRA-sim
${ASTRASIM_DIR:?}/build/astra_analytical/build.sh

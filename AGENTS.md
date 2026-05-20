## Cursor Cloud specific instructions

ASTRA-sim is a C++17 distributed AI system simulator. It is not a web service; it produces command-line executables that run simulations.

### Key commands

| Action | Command |
|---|---|
| Build (analytical backend) | `./build/astra_analytical/build.sh` |
| Clean + rebuild | `./build/astra_analytical/build.sh -l && ./build/astra_analytical/build.sh` |
| Run regression tests | `./tests/run_all.sh` |
| Lint (clang-format) | `clang-format --dry-run --Werror <file>` |
| Run example (congestion unaware) | `./examples/run_scripts/analytical/congestion_unaware/Ring_reducescatter_4npus.sh` |
| Run example (congestion aware) | `./examples/run_scripts/analytical/congestion_aware/Ring_allgather_16npus.sh` |
| Build (ns-3 backend) | `./build/astra_ns3/build.sh -c` |
| Clean ns-3 build | `./build/astra_ns3/build.sh -l` |
| Run ns-3 example | `./examples/run_scripts/ns3/Ring_allgather_16npus.sh` |

### CMake layout (AstraSim core vs backends)

- **Shared library build**: `cmake/AstraSimLibrary.cmake` builds `AstraSim` (fmt, spdlog, bundled protobuf/Abseil, Chakra `et_def` codegen). Set `ASTRA_SIM_ROOT` to the repo root, then `include(.../cmake/AstraSimLibrary.cmake)`. If `AstraSim` already exists, the include is a no-op.
- **Backends do not `add_subdirectory` the repo root**: `build/astra_analytical/`, `build/astra_htsim/`, and `extern/network_backend/ns-3/scratch/CMakeLists.txt` all use `AstraSimLibrary.cmake` instead of pulling in the root `CMakeLists.txt` project. Backend-only CMake changes (e.g. analytical network parser) stay in their submodules.
- **Optional standalone**: Root `CMakeLists.txt` only builds the `AstraSim` static library (`ASTRA_SIM_STANDALONE_BUILD=ON`).
- **Bundled protobuf (default)**: `extern/helper/abseil-cpp` and `extern/helper/protobuf`; disable with `-DASTRA_SIM_USE_BUNDLED_PROTOBUF=OFF` for system protobuf. Executables may need to link `AstraSimProtobufDeps` when bundled (static link propagation is incomplete).

### Non-obvious gotchas

- **Protobuf version mismatch**: The Chakra submodule's Python package declares `protobuf==5.*` but its generated code (`et_def_pb2.py`) requires protobuf >= 6.x at runtime. After `pip install extern/graph_frontend/chakra`, run `pip install "protobuf>=6.31.0,<7"` to resolve the mismatch. The incompatibility warning from pip is expected and harmless.
- **libstdc++-14-dev required**: On Ubuntu 24.04 the default C++ compiler (clang 18) links against GCC 14's libstdc++. You must have `libstdc++-14-dev` installed or the CMake compiler check will fail with `cannot find -lstdc++`.
- **Git submodules must be initialized**: Most extern code (chakra, fmt, spdlog, ns-3, etc.) lives in git submodules. Run `git submodule update --init --recursive` before building. The **analytical network backend** (`extern/network_backend/analytical`) is vendored in-tree (JSON config, no separate submodule) because upstream `astra-network-analytical` does not publish the required commit.
- **Chakra submodule fetch may fail**: The Chakra submodule points to a fork (`cho445184-eng/chakra.git`) that may not contain the exact pinned commit (force-pushed away). If `git submodule update --init` fails for chakra, manually clone the fork's main branch into `extern/graph_frontend/chakra/`: `rm -rf extern/graph_frontend/chakra && git clone https://github.com/cho445184-eng/chakra.git extern/graph_frontend/chakra`.
- **Submodule working tree restoration**: After submodule init, some submodules may show all files as "deleted" in git status. Run `git restore --staged . && git restore .` inside each affected submodule directory to restore files.
- **Build artifacts location**: The analytical backend binaries are at `build/astra_analytical/build/bin/AstraSim_Analytical_Congestion_Aware` and `build/astra_analytical/build/bin/AstraSim_Analytical_Congestion_Unaware`.
- **Regression test workload generation**: Tests use `python3` to generate Chakra trace files via `tests/rt_template/inputs/workload/gen_chakra_traces.py`, which depends on the Chakra Python package.
- **ns-3 backend requires OpenMPI**: Install `openmpi-bin openmpi-doc libopenmpi-dev` before building the ns-3 backend. The ns-3 build takes ~2 minutes.
- **ns-3 libbgp submodule**: BGP/RDMA routing requires [Nat-Lab/libbgp](https://github.com/Nat-Lab/libbgp) (pinned 0.6.3) at `extern/network_backend/ns-3/src/bgp/libbgp`. It is built automatically by the ns-3 `src/bgp` CMake module; no system `libbgp` install is needed. After cloning, run `git submodule update --init --recursive` (or `git submodule update --init src/bgp/libbgp` inside the ns-3 tree).
- **Submodule update**: Run `git submodule update --init --recursive` from the repo root. Nested submodules include `extern/helper/protobuf/third_party/*` and `extern/network_backend/ns-3/src/bgp/libbgp`. If ns-3 checkout fails due to `scratch/output/flow.txt`, remove that file (runtime artifact; gitignored) and re-run update.
- **ns-3 flow.txt prerequisite**: Before running ns-3 examples, the file `extern/network_backend/ns-3/scratch/output/flow.txt` must exist with content `0` (zero flows). Without it the simulation fails with "cannot open flow file". Create it with `echo "0" > extern/network_backend/ns-3/scratch/output/flow.txt`.
- **ns-3 binary location**: The ns-3 binary is at `extern/network_backend/ns-3/build/scratch/ns3.42-AstraSimNetwork-default`. Examples must be run from the `build/scratch/` directory.
- **HTSim backend is optional**: Only analytical and ns-3 backends are needed for CI tests.
- **Multi-dimensional topologies require congestion-unaware backend**: The `AstraSim_Analytical_Congestion_Aware` binary only supports 1-dim topologies. For multi-dimensional CLOS (2-layer or 3-layer), use `AstraSim_Analytical_Congestion_Unaware`.
- **Workload generator PYTHONPATH**: The scripts in `examples/workload/microbenchmarks/generator_scripts/` use `from extern.graph_frontend.chakra...` imports. Set `PYTHONPATH=/workspace` (project root) when invoking them, or they will fail with `ModuleNotFoundError`.
- **CLOS topology generation**: Use `examples/network/analytical/generator_scripts/generate_clos_topology.py` for 2-layer (`--layers 2`) and 3-layer (`--layers 3`) CLOS analytical topologies. Run all CLOS simulations via `examples/run_scripts/analytical/congestion_unaware/run_clos_simulations.sh`.
- **ns-3 CLOS topology generation**: Use `examples/network/ns3/generator_scripts/generate_ns3_clos_topology.py` for ns-3 physical CLOS topologies. Run all ns-3 CLOS simulations via `examples/run_scripts/ns3/run_ns3_clos_simulations.sh`.
- **ns-3 large-scale AllToAll is slow**: The 512-node AllToAll simulation over ns-3 requires extremely long runtime and >10GB RAM due to O(n^2) packet-level simulation. AllReduce scales much better. For quick verification, prefer AllReduce or use smaller collective sizes for AllToAll.

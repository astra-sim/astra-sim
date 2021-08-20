# build/analytical

## What is this directory for?
Astra-sim has an [Analytical Backend](https://github.com/astra-sim/analytical) support.
This directory contains compilation scripts for you to build, link, and run this backend.

## Dependencies
Analytical Backend uses `cmake` as a build manager and requires `boost` library when compiling.
On macOS, you may also require `coreutils` package to correctly locate build scripts.
Please install these required packages on your machine.
Below are sample runs:
### macOS
```bash
brew install cmake boost coreutils
```
### Linux (Debian/Ubuntu)
```bash
sudo apt install cmake libboost-all-dev
```

## Compilation
After installing the required dependencies, you can compile both Astra-sim and Analytical Backend together by running a build script provided in this directory.
```bash
./build.sh -c
```

## Run
After a successful compilation, you can run the compiled binary.
- Check [this page](https://github.com/astra-sim/astra-sim) for available Astra-sim configurations.
- Check [this page](https://github.com/astra-sim/analytical) for Analytical Network configurations.

## Cleanup
For your convenience, the build script provides you sugar for easily removing compiled binary and related build files.
```bash
./AnalyticalBackend.sh -l
```

To remove both build files and run results, use `-lr` option.
```bash
./AnalyticalBackend.sh -lr
```

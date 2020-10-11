<!--
Copyright (c) 2020 Georgia Institute of Technology
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Author : William Won (william.won@gatech.edu)
-->

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
After a successful compilation, you can run the compiled binary by using the same build script file.
```bash
./build.sh -r
```
You can also configure the run by passing command-line arguments and/or changing the network configuration `.json` file.
- Check [this page](https://github.com/astra-sim/astra-sim) for available Astra-sim configurations.
- Check [this page](https://github.com/astra-sim/analytical) for Analytical Network configurations.

## Cleanup
For your convenience, the build script provides you sugar for easily removing compiled binary and related build files.
```bash
./AnalyticalBackend.sh -l
```

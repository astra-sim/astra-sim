# Chakra traces for e2e workload

This directory provides example Chakra traces for end-to-end workloads.
Use the sample scripts below to download and combine a starter trace.
For additional traces, see: https://github.com/mlcommons/chakra/wiki/Chakra-Trace-Library

Run:

```bash
pip install requirements.txt
bash download_nemo_chakra_traces.sh
bash combine_trace.sh
```
The pip requirements are needed to download files from a google drive link, and create the combined chakra traces. The google drive will download the kineto and PyTorch ET files.

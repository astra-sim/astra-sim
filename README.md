# Announcement

There are two known issues caused by the upstream tool. Please follow the temporary solutions below to resolve them:

### Issue #1
Simulation appears as if communication is not happening. This is because `comm_size` is falsely set to zero due to mismatched `protobuf` types (`uint64` vs `int64`).

**Solution:**  
Apply the patch by running the following command at the project directory: 

```
git apply --directory extern/graph_frontend/chakra comm_size_var_type_fix.patch
```

### Issue #2
The installation of Chakra tool strictly enforces a `protobuf` version that is broken and requires an update.

**Solution:**  
After installing Chakra, update `protobuf` to version *5.27.2* by running:  

```
pip3 install protobuf==5.27.2
```

> **_NOTE:_** We are closely monitoring the issues caused by the upstream tool and will provide updates as soon as they are resolved.


# ASTRA-sim 2.0
[ASTRA-sim](https://astra-sim.github.io/) is a distributed machine learning system simulator developed by Intel, Meta, and Georgia Tech. It enables the systematic study of challenges in modern deep learning systems, allowing for the exploration of bottlenecks and the development of efficient methodologies for large DNN models across diverse future platforms.

The previous version, ASTRA-sim 1.0, is available in the `ASTRA-sim-1.0` [branch](https://github.com/astra-sim/astra-sim/tree/ASTRA-sim-1.0).

Here is a concise visual summary of our simulator:
![alt text](https://github.com/astra-sim/astra-sim/blob/master/docs/images/astrasim_overview_codesign.png)

For a comprehensive understanding of the tool, and to gain insights into its capabilities, please visit our [website](https://astra-sim.github.io/).

For information on how to use ASTRA-sim, please visit our [Wiki](https://astra-sim.github.io/astra-sim-docs/index.html).

ASTRA-sim accepts Chakra Execution Traces as workload-layer inputs. For details, please visit [Chakra Github](https://github.com/mlcommons/chakra).

We appreciate your interest and support in ASTRA-sim!

## Contact Us
For any questions about using ASTRA-sim, you can email the ASTRA-sim User Mailing List: astrasim-users@googlegroups.com

To join the mailing list, please fill out the following form: https://forms.gle/18KVS99SG3k9CGXm6

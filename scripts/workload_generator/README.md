## Generate ASTRA-Sim Workload Input File

The `gen_astrasim_workload_input.py` can be run to generate the workload input file for ASTRA-Sim. It takes multiple inputs arguments specified as follows:

- `datatype_size`: the size of each data element in bytes
- `mnk`: the path of input file that has the m,n,k values (described later)
- `num_npu`: total number of NPUs in the system
- `num_packages`: number of packages in the system
- `output_file`: the file name and path for the generated ASTRA-Sim input file
- `parallel`: parallelization strategy, the options are listed bellow:
  - *DATA*: data-parallelism
  - *MODEL*: model-parallelism
  - *HYBRID_DATA_MODEL*: data-parallelism between packages and model-parallelism within package
  - *HYBRID_MODEL_DATA*: model-parallelism between packages and data-parallelism within package
  - *HYBRID_CUSTOMIZED*: customized parallelism for each layer, see [mnk input file format](#customized-parallelisms-for-each-layer)

- `run_name`: the folder name that holds the genreated output from SCALE-Sim
- `scalesim_config`: path to a SCALE-Sim config file
- `scalesim_path`: path to the SCALE-Sim folder

Some examples:

```bash
# For data-parallel
$ python3 gen_astrasim_workload_input.py \
  --datatype_size=2 \
  --mnk=mnk_inputs/example.csv \
  --num_npus=16 \
  --num_packages=2 \
  --output_file=../../inputs/workload/example_DATA.txt \
  --parallel=DATA \
  --run_name=example \
  --scalesim_config=../../extern/compute/SCALE-Sim/configs/google.cfg \
  --scalesim_path=../../extern/compute/SCALE-Sim

# For customized parallelism
$ python3 gen_astrasim_workload_input.py \
  --datatype_size=2 \
  --mnk=mnk_inputs/example_customized.csv \
  --num_npus=16 \
  --num_packages=2 \
  --output_file=../../inputs/workload/example_HYBRID_CUSTOMIZED.txt \
  --parallel=HYBRID_CUSTOMIZED \
  --run_name=example_customized \
  --scalesim_config=../../extern/compute/SCALE-Sim/configs/google.cfg \
  --scalesim_path=../../extern/compute/SCALE-Sim

# Using the customized mnk file for data-parallel
# 	- customization is overrided by "--parallel=DATA"
$ python3 gen_astrasim_workload_input.py \
  --datatype_size=2 \
  --mnk=mnk_inputs/example_customized.csv \
  --num_npus=16 \
  --num_packages=2 \
  --output_file=../../inputs/workload/example_DATA.txt \
  --parallel=DATA \
  --run_name=example \
  --scalesim_config=../../extern/compute/SCALE-Sim/configs/google.cfg \
  --scalesim_path=../../extern/compute/SCALE-Sim
```

**Dependent python version and packages:**

- Python3
- absl-py
- os
- subprocess
- math
- configparser
- tqdm

**Note**: Currently, the script only supports GEMM type layers. We plan to add supports for other layer types suchs Embedding in the future.



## MNK Input File Format

### Same Parallelism for All The Layers

An example mnk input file for *DATA, MODEL, HYBRID_DATA_MODEL, HYBRID_MODEL_DATA* parallelism is `mnk_inputs/example.csv`.

The format of mnk input file is as follows:

* **first line**: Layer,m,n,k

* **subsequent lines**: Each subsequent line describes the GEMM of a layer.

  The format of a layer is as follows:

  * (string: **layer name**),(int: **m**),(int: **n**),(int: **k**)

  The descriptioin of each parameter for a layer in a line is as follows:

  * *layer name*: the name of the layer
  * *m*: number of inputs (minibatch), can be sharded for data-parallelism
  * *n*: output dimension, can be sharded for model-parallelism
  * *k*: input dimension

### Customized Parallelisms for Each Layer

An example mnk input file for *HYBRID_CUSTOMIZED* parallelism is `mnk_inputs/example_customized.csv`.

**NOTE**: customized mnk input file can also be used for same parallelism for all the layers by setting `--parallel=[DATA|MODEL|HYBRID_DATA_MODEL|HYBRID_MODEL_DATA]` (one of the four options).

The format of mnk input file is as follows:

* **first line**: Layer,m,n,k,CustomizedParallelization

* **subsequent lines**: Each subsequent line describes the GEMM of a layer.

  The format of a layer is as follows:

  * (string: **layer name**),(int: **m**),(int: **n**),(int: **k**),(string: **paralleism**)

  The descriptioin of each parameter for a layer in a line is as follows:

  * *layer name*: the name of the layer
  * *m*: number of inputs (minibatch), can be sharded for data-parallelism
  * *n*: output dimension, can be sharded for model-parallelism
  * *k*: input dimension
  * *parallelism*: The parallelization strategy for this layer (options: *DATA, MODEL, HYBRD_DATA_MODEL, HYBRID_MODEL_DATA*)

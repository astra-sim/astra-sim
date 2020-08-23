#### Generate ASTRA-Sim Workload Input File

The `gen_astrasim_workload_input.py` can be run to generate the workload input file for ASTRA-Sim. It takes multiple inputs arguments specified as follows:

- `datatype_size`: the size of each data element in bytes
- `mnk`: the path of input file that has the m,n,k values (described later)
- `num_npu`: total number of NPUs in the system
- `num_packages`: number of packages in the system
- `output_file`: the file name and path for the generated ASTRA-Sim input file
- `parallel`: parallelization strategy, the options are listed bellow:
  - *DATA*: data-parallelism
  - *Model*: model-parallelism
  - *HYBRID_DATA_MODEL*: data-parallelism between packages and model-parallelism within package
  - *HYBRID_MODEL_DATA*: model-parallelism between packages and data-parallelism within package

- `run_name`: the folder name that holds the genreated output from SCALE-Sim
- `scalesim_config`: path to a SCALE-Sim config file
- `scalesim_path`: path to the SCALE-Sim folder

An example run:

```bash
$ python gen_astrasim_workload_input.py \
  --datatype_size=2 \
  --mnk=mnk_inputs/test.csv \
  --num_npus=16 \
  --num_packages=2 \
  --output_file=../workload_inputs/test_DATA.txt \
  --parallel=DATA \
  --run_name=test \
  --scalesim_config=../../compute/SCALE-Sim/configs/google.cfg \
  --scalesim_path=../../compute/SCALE-Sim
```

**Note**: Currently, the script only supports GEMM type layers and the same parallelization strategy for all the layers. We plan to add supports for other layer types suchs Embedding and separate parallelization strategies for each layer in the future.



#### MNK Input File Format

An example mnk input file is in `mnk_inputs/test.csv`

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


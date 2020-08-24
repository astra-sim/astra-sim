# ASTRA-Sim #

### What is this repository for? ###
This is the ASTRA-sim distributed Deep Learning Training simulator, developed in collaboration between Georgia Tech, Facebook and Intel.

An overview is presented here:
![alt text](https://github.com/astra-sim/astra-sim/blob/master/docs/images/astrasim_overview_codesign.png)

The full description of the tool and its strength can be found in the paper below:

Saeed Rashidi, Srinivas Sridharan, Sudarshan Srinivasan, and Tushar Krishna,
"ASTRA-SIM: Enabling SW/HW Co-Design Exploration for Distributed DL Training Platforms"
*In Proc of the IEEE International Symposium on Performance Analysis of Systems and Software (ISPASS), Apr 2020*
[[pdf]](https://synergy.ece.gatech.edu/wp-content/uploads/sites/332/2020/03/astrasim_ispass2020.pdf)[slides - coming soon]

**Bibtex**

    @inproceedings{astrasim,
        author       = {Saeed Rashidi and
                       Srinivas Sridharan and
                       Sudarshan Srinivasan and
                       Tushar Krishna},
        title        = {{ASTRA-SIM: Enabling SW/HW Co-Design Exploration for Distributed DL Training Platforms}},
        booktitle     = {{IEEE} International Symposium on Performance Analysis of Systems
                        and Software, {ISPASS} 2020, Boston, MA, USA, August 22-26, 2020},
      publisher     = {{IEEE}},
      year          = {2020},
    }


### Setup Instructions ###

```bash
# Clone the repository
$ git clone https://github.com/astra-sim/astra-sim.git

# Build the repository
#  - You will be asked what backend to downlaod: gem5 or ns3
#  - The SCALE-Sim compute model will be cloned to the compute folter
$ cd astra-sim
$ ./build.sh
```

#### Instructions for running Garnet2.0 as network simulator
1. Enter gem5 when prompted by build.sh. This will clone the https://github.com/georgia-tech-synergy-lab/gem5_astra repository inside the network folder
2. Go to network/gem5_astra
3. Run: "./my_scripts/build_Garnet_standalone.sh" to build the garnet network simulator.
4. Go to astra_runs/ directory
5. Run: "./sampleDLRM_a2a.sh runName" where runName is an arbitrary name you choose for this specific run. This will run a toy DLRM model over a physical alltoall topology.
6. After the sim finishes, go to ../astra_results/runName-a2a directory, the detailed and EndToEnd csv files for this run are stored there (times are in microseconds in the csv files)

NOTE: you can also run "./sampleDLRM_torus.sh runName" for step 5 instead. This will run a toy DLRM model over a physical 3D torus.

NOTE: The on-screen reported delays after the end of simulation are in cycles while the delays inside the csv files are in terms of microSeconds.


#### Instructions for running NS3 as network simulator
Coming Soon!

### Input Files to ASTRA-sim ###

* Workload: workload/workload_inputs/
   * see workload_inputs/README.md
   * see workload_generator/README.md
* System: system/system_inputs/
   * see system_inputs/README.md
* Network: network/gem5_astra/network_inputs/
   * see network_inputs/README.md

### Contact ###
Please email Saeed Rashidi (saeed.rashidi@gatech.edu) or Srinivas Sridharan (ssrinivas@fb.com) or Tushar Krishna (tushar@ece.gatech.edu) if you have any questions.

### Core Developers ###
* Saeed Rashidi (Georgia Tech)
* Srinivas Sridharan (Facebook)

### External Contributors ###
* Jiayi Huang (University of California, Santa Barbara)
* Apurve Chawde (Georgia Tech)
* Santosh Kumar Elangoven (Georgia Tech)

# README #

### What is this repository for? ###
This is the ASTRA-sim distributed Deep Learning Training simulator. 

The full description of the tool and its strength can be found in the paper below:

Saeed Rashidi, Srinivas Sridharan, Sudarshan Srinivasan, and Tushar Krishna, "ASTRA-SIM: Enabling SW/HW Co-Design Exploration for Distributed DL Training Platforms"
*In Proc of the IEEE International Symposium on Performance Analysis of Systems and Software (ISPASS), Apr 2020*
[[pdf]](https://synergy.ece.gatech.edu/wp-content/uploads/sites/332/2020/03/astrasim_ispass2020.pdf).

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

Clone the repository
Run ./build.sh --> you will be asked what backend to download: gem5 or ns3

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
1. Enter ns3 when prompted by build.sh. Note that at this time, this repository is private and ONLY those who have access can clone it.
2. Go to ns3_astra/ns-3.30/
3. Run: "./waf configure"
3. Run some experimnets to build the entire NS3. All results will be dumped inside the "results" directory, a sample experiment is :

***./waf --run "dlrm_workload_rdma --nHosts=128 --workload=microAllReduce  --nPasses=1 --MTUB=1460  --n2nSpeedGbps=100 --localDim=8 --nSwitches=1 --n2nDlyNs=500  --commScale=0.01 --totalStat=1 --statRow=0 --sys=sample_a2a_sys  --runName=run1"***

6. Now you can run either some other experiments or the available runscripts that run bunch of experiments simultaneously

NOTE: The on-screen reported delays after the end of simulation are in cycles while the delays inside the csv files are in terms of microSeconds.


### Input Files to ASTRA-sim ###

* Workload: workload/workload_inputs/
   * see workload_inputs/README.md
* System: system/system_inputs/
   * see system_inputs/README.md
* Network: network/gem5_astra/network_inputs/ or network/ns3_astra/network_inputs/
   * see network_inputs/README.md

### Contact ###
Please email Saeed Rashidi (saeed.rashidi@gatech.edu) or Srinivas Sridharan (ssrinivas@fb.com) if you have any questions.

### Developers and Contributors ###
**Georgia Tech**
* Saeed Rashidi
* William Won
* Tushar Krishna

**Facebook**
* Srinivas Sridharan
* Pallavi Shurpali

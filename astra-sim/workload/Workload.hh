/******************************************************************************
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

Author : Saeed Rashidi (saeed.rashidi@gatech.edu)
*******************************************************************************/
#ifndef __WORKLOAD_HH__
#define __WORKLOAD_HH__


#include <map>
#include <math.h>
#include <fstream>
#include <chrono>
#include <ctime>
#include <tuple>
#include <cstdint>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "astra-sim/system/Callable.hh"

namespace AstraSim{
class Workload;
class Sys;
class Callable;
class Layer;
class CSVWriter;
}

#include "astra-sim/system/AstraSimDataAPI.hh"
#include "astra-sim/system/Sys.hh"

namespace AstraSim{
enum class ParallelismPolicy {MicroBenchmark,Data,Transformer,TransformerFwdInBckwd,DLRM,DLRMEnhanced,Model,HybridDataModel,HybridModelData,HybridCustomized,DistributedInference,None};
#define FREQ (1000.0/CLOCK_PERIOD)

class Workload:Callable {
public:
    enum class LoopState {
        Forward_Pass, Weight_Gradient, Input_Gradient,Wait_For_Sim_Finish, Forward_In_BackPass
    };
    ~Workload();
    Layer **layers;
    int SIZE;
    Sys *generator;
    std::string run_type;
    Tick counter;
    int index;
    LoopState current_state;
    bool delay_loaded;
    bool seprate_log;
    bool checkpoint_initiated;
    bool collective_issued;
    bool initialized;
    int TOTAL_PASS;
    int DLRM_LAST_BOTTOM_LAYER;
    int pass_counter;
    int pending_collectives;
    ParallelismPolicy parallelismPolicy;
    //reports
    Tick waiting_for_comm;
    Workload(std::string run_name,Sys *generator, std::string name, int TOTAL_PASS,int total_rows,int stat_row,std::string path,bool seprate_log);
    ParallelismPolicy decode_parallelsim(std::string parallelism);
    void call(EventType event, CallData *data);
    void iterate_micro_benchmark();
    void iterate_data_parallel();
    void iterate_hybrid_parallel_Transformer();
    void iterate_hybrid_parallel_Transformer_fwd_in_bckwd();
    void iterate_hybrid_parallel_DLRM();
    void iterate_hybrid_parallel_data_model();
    void iterate_hybrid_parallel_model_data();
    void iterate_hybrid_parallel_customized();
    void iterate_model_parallel();
    void iterate_distributed_inference();
    bool initialize_workload(std::string name);
    void initialize_stat_files();
    void fire();
    void report();
    void check_for_sim_end();
    static int get_layer_numbers(std::string workload_input);
    CSVWriter *detailed;
    CSVWriter *end_to_end;
    std::string path;
    std::string run_name;
    int stat_row;
    int total_rows;
    bool registered_for_finished_streams;
};
}
#endif

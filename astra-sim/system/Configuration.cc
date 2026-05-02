/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include <iostream>
#include <fstream>
#include "string.h"

#include "astra-sim/common/Logging.hh"
#include "Sys.hh"
#include <json/json.hpp>
#include <yaml-cpp/yaml.h>

using namespace std;
using json = nlohmann::json;

namespace AstraSim {
string Sys::Configuration::GetConfigType()
{
    return FileType;
}

void Sys::Configuration::ConfigPanic(string msg) {
    auto logger = LoggerFactory::get_logger("Configuration");
    logger->critical(msg);
    exit(1);
}
    
bool Sys::Configuration::ExtractJsonConfigParams(string filename) {
    ifstream inFile;
    inFile.open(filename);
    if (!inFile) {
        if (sys->id == 0) {
            LoggerFactory::get_logger("Configuration")->critical(
                "Unable to open file: {}", filename);
        }
        exit(1);
    }
    json j;
    inFile >> j;
    if(j.contains("frequency")) {
        sys->frequency = j["frequency"];
    }
    if (j.contains("scheduling-policy")) {
        string inp_scheduling_policy = j["scheduling-policy"];
        if (inp_scheduling_policy == "LIFO") {
            sys->scheduling_policy = SchedulingPolicy::LIFO;
        } else if (inp_scheduling_policy == "FIFO") {
            sys->scheduling_policy = SchedulingPolicy::FIFO;
        } else if (inp_scheduling_policy == "EXPLICIT") {
            sys->scheduling_policy = SchedulingPolicy::EXPLICIT;
        } else {
            ConfigPanic("unknown value for scheduling policy in sys input file");
        }
    }
    if (j.contains("collective-optimization")) {
        string inp_collective_optimization = j["collective-optimization"];
        if (inp_collective_optimization == "baseline") {
            sys->collectiveOptimization = CollectiveOptimization::Baseline;
        } else if (inp_collective_optimization == "localBWAware") {
            sys->collectiveOptimization = CollectiveOptimization::LocalBWAware;
        } else {
            ConfigPanic(
                "unknown value for collective optimization in sys input file");
        }
    }
    if (j.contains("local-reduction-delay")) {
        sys->local_reduction_delay = j["local-reduction-delay"];
    }
    if (j.contains("active-chunks-per-dimension")) {
        sys->active_chunks_per_dimension = j["active-chunks-per-dimension"];
    }
    if (j.contains("L")) {
        sys->inp_L = j["L"];
    }
    if (j.contains("o")) {
        sys->inp_o = j["o"];
    }
    if (j.contains("g")) {
        sys->inp_g = j["g"];
    }
    if (j.contains("G")) {
        sys->inp_G = j["G"];
    }
    if (j.contains("endpoint-delay")) {
        sys->communication_delay = j["endpoint-delay"];
        sys->communication_delay = sys->communication_delay * sys->injection_scale;
    }
    sys->model_shared_bus =    j.contains("model-shared-bus") ? 
                        j["model-shared-bus"] ? true : false 
                        : false;
    
    if (j.contains("preferred-dataset-splits")) {
        sys->preferred_dataset_splits = j["preferred-dataset-splits"];
    }
    if (j.contains("peak-perf")) {
        sys->peak_perf = j["peak-perf"];
        sys->peak_perf *= 1000000000000;  // TFLOPS
    }
    if (j.contains("local-mem-bw")) {
        sys->local_mem_bw = j["local-mem-bw"];
        sys->local_mem_bw *= 1000000000; // GB/sec
    }
    if (j.contains("roofline-enabled")) {
        if (j["roofline-enabled"] != 0) {
            sys->roofline_enabled= true;
        }
    }
    sys->trace_enabled =  j.contains("trace-enabled") ? 
                    j["trace-enabled"] ? true : false 
                    : false;

    sys->replay_only =   j.contains("replay-only") ? 
                    (j["replay-only"]!=0) ? true : false 
                    : false;

    sys->track_local_mem = j.contains("track-local-mem") ? 
                    (j["track-local-mem"]!=0) ? true : false 
                    : false;

    sys->local_mem_trace_filename = "local_mem_trace";
    if (j.contains("local-mem-trace-filename")) {
        sys->local_mem_trace_filename= j["local-mem-trace-filename"];
    }

    sys->collective_impl_lookup->setup_collective_impl_from_config(j);


    inFile.close();
    return true;
}

bool Sys::Configuration::ExtractYamlConfigParams(string filename)
{
    YAML::Node y;
    try {
        y = YAML::LoadFile(filename);
    } catch (const YAML::BadFile& e) {
        throw runtime_error("Failed to open YAML file: " + filename);
    } catch (const YAML::ParserException& e) {
        throw runtime_error(string("YAML parse error: ") + e.what());
    }
    if (!y || !y.IsMap()) {
        throw runtime_error("Top-level YAML must be a mapping of int -> string");
    }

    if(y["frequency"]) {
        sys->frequency = y["frequency"].as<double>();
    }
    string inp_scheduling_policy = y["scheduling-policy"].as<string>();
    if (inp_scheduling_policy == "LIFO") {
        sys->scheduling_policy = SchedulingPolicy::LIFO;
    } else if (inp_scheduling_policy == "FIFO") {
        sys->scheduling_policy = SchedulingPolicy::FIFO;
    } else if (inp_scheduling_policy == "EXPLICIT") {
        sys->scheduling_policy = SchedulingPolicy::EXPLICIT;
    } else {
        ConfigPanic("unknown value for scheduling policy in sys input file");
    }
    

    string inp_collective_optimization = y["collective-optimization"].as<string>();
    if (inp_collective_optimization == "baseline") {
        sys->collectiveOptimization = CollectiveOptimization::Baseline;
    } else if (inp_collective_optimization == "localBWAware") {
        sys->collectiveOptimization = CollectiveOptimization::LocalBWAware;
    } else {
        ConfigPanic(
            "unknown value for collective optimization in sys input file");
    }

    if (y["local-reduction-delay"]) {
        sys->local_reduction_delay = y["local-reduction-delay"].as<int>();
    }
    if (y["active-chunks-per-dimension"]) {
        sys->active_chunks_per_dimension = y["active-chunks-per-dimension"].as<int>();
    }
    if (y["L"]) {
        sys->inp_L = y["L"].as<float>();
    }
    if (y["o"]) {
        sys->inp_o = y["o"].as<float>();
    }
    if (y["g"]) {
        sys->inp_g = y["g"].as<float>();
    }
    if (y["G"]) {
        sys->inp_G = y["G"].as<float>();
    }
    if (y["endpoint-delay"]) {
        sys->communication_delay = y["endpoint-delay"].as<int>();
        sys->communication_delay = sys->communication_delay * sys->injection_scale;
    }  
    sys->model_shared_bus =    y["model-shared-bus"] ? 
                        y["model-shared-bus"].as<bool>() ? true : false 
                        : false;
    
    if (y["preferred-dataset-splits"]) {
        sys->preferred_dataset_splits = y["preferred-dataset-splits"].as<int>();
    }
    if (y["peak-perf"]) {
        sys->peak_perf = y["peak-perf"].as<double>();
        sys->peak_perf *= 1000000000000;  // TFLOPS
    }
    if (y["local-mem-bw"]) {
        sys->local_mem_bw = y["local-mem-bw"].as<double>();
        sys->local_mem_bw*= 1000000000; // GB/sec
    }
    if (y["roofline-enabled"]) {
        sys->roofline_enabled = y["roofline-enabled"].as<bool>();
    }
    if(y["trace-enabled"]) {
        sys->trace_enabled = y["trace-enabled"].as<bool>();
    }
    if(y["replay-only"]) {
        sys->replay_only = y["replay-only"].as<bool>();
    }
    if(y["track-local-mem"]) {
        sys->track_local_mem = y["track-local-mem"].as<bool>();
    }

    sys->local_mem_trace_filename = "local_mem_trace";
    if (y["local-mem-trace-filename"]) {
        sys->local_mem_trace_filename= y["local-mem-trace-filename"].as<string>();
    }
    // TODO add collective parameters for YAML
    //collectiveImplLookup->setup_collective_impl_from_config(j);
    return true;

}
}   //namespace AstraSim

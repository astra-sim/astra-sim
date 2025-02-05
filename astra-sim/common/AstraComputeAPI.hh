/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __COMPUTE_HH__
#define __COMPUTE_HH__
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include "astra-sim/system/Common.hh"
#include "astra-sim/system/AstraNetworkAPI.hh"

namespace AstraSim {
class Sys;
enum class ComputeKernelType{GEMM, Batch_GEMM, Softmax, LLM_RMSNorm, LLM_Residual_Addition, Llama_Attn, Llama_MLP};
enum class ComputeKernelPhase{FWD,BCKWD, INFERENCE_TOKENGEN, INFERENCE_PREFILL};

class ComputeKernel{
  public:
    ComputeKernelType type;
    ComputeKernelPhase phase;
    std::unordered_map<std::string,std::string> attributes;
    ComputeKernel(ComputeKernelType type,ComputeKernelPhase phase,std::unordered_map<std::string,std::string> attributes){
      this->type=type;
      this->phase=phase;
      this->attributes=attributes;
    }
    ComputeKernel(){};
    static ComputeKernel create_llm_rmsnorm(int batch, int seq_len, int n_embed, ComputeKernelPhase phase);
    static ComputeKernel create_llama_attn(int batch, int seq_len, int max_seq_len, int n_embed, int num_heads, int tensor_parallelism, ComputeKernelPhase phase);
    static ComputeKernel create_llm_residual_addition(int batch, int seq_len, int n_embed, ComputeKernelPhase phase);
    static ComputeKernel create_llama_mlp(int batch, int seq_len, int n_embed, int num_heads, int tensor_parallelism, ComputeKernelPhase phase);
};
class ComputeKernelSimulationMetaData {
 public:
  timespec_t compute_delay; // delay in nanoseconds
};
class AstraComputeAPI {
 public:
  
  // gets the runtime in a blocking manner
  virtual timespec_t get_static_runtime(
    ComputeKernel &kernel
  ) {timespec_t t; return t;};

  // simulates the runtime asynchronously and calls the system layer once it is done
  virtual void simulate(
      ComputeKernel &kernel,
      Sys *sys,
      void (*msg_handler)(void* fun_arg),
      ComputeKernelSimulationMetaData* fun_arg) {return;};

  //virtual ~AstraComputeAPI() = 0;
};
} // namespace AstraSim
#endif

/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "AstraComputeAPI.hh"

using namespace AstraSim;

ComputeKernel ComputeKernel::create_llm_rmsnorm(int batch, int seq_len, int n_embed, ComputeKernelPhase phase){
  std::unordered_map<std::string, std::string> attr={{"batch", std::to_string(batch)}, {"seq_len", std::to_string(seq_len)},
   {"n_embed", std::to_string(n_embed)}};
  return ComputeKernel(ComputeKernelType::LLM_RMSNorm,phase,attr);
};
ComputeKernel ComputeKernel::create_llm_residual_addition(int batch, int seq_len, int n_embed, ComputeKernelPhase phase){
  std::unordered_map<std::string, std::string> attr={{"batch", std::to_string(batch)}, {"seq_len", std::to_string(seq_len)},
   {"n_embed", std::to_string(n_embed)}};
  return ComputeKernel(ComputeKernelType::LLM_Residual_Addition,phase,attr);
};
ComputeKernel ComputeKernel::create_llama_attn(int batch, int seq_len, int max_seq_len, int n_embed, int num_heads, int tensor_parallelism, ComputeKernelPhase phase){
  std::unordered_map<std::string, std::string> attr={{"batch", std::to_string(batch)}, {"seq_len", std::to_string(seq_len)}, {"max_seq_len", std::to_string(max_seq_len)},
   {"n_embed", std::to_string(n_embed)}, {"num_heads", std::to_string(num_heads)}, {"tensor_parallelism", std::to_string(tensor_parallelism)}};
  return ComputeKernel(ComputeKernelType::Llama_Attn,phase,attr);
};
ComputeKernel ComputeKernel::create_llama_mlp(int batch, int seq_len, int n_embed, int num_heads, int tensor_parallelism, ComputeKernelPhase phase){
  std::unordered_map<std::string, std::string> attr={{"batch", std::to_string(batch)}, {"seq_len", std::to_string(seq_len)},
   {"n_embed", std::to_string(n_embed)}, {"num_heads", std::to_string(num_heads)}, {"tensor_parallelism", std::to_string(tensor_parallelism)}};
  return ComputeKernel(ComputeKernelType::Llama_MLP,phase,attr);
};
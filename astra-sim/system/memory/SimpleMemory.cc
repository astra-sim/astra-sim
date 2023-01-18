/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/memory/SimpleMemory.hh"

using namespace AstraSim;

SimpleMemory::SimpleMemory(
    AstraNetworkAPI* comm_NI,
    double access_latency,
    double npu_access_bw_GB,
    double nic_access_bw_GB) {
  this->comm_NI = comm_NI;
  this->last_read_request_serviced = 0;
  this->last_write_request_serviced = 0;
  this->npu_access_bw_GB = npu_access_bw_GB;
  this->nic_access_bw_GB = nic_access_bw_GB;
  this->access_latency = access_latency;
  this->nic_read_request_count = 0;
  this->nic_write_request_count = 0;
  this->npu_read_request_count = 0;
  this->npu_write_request_count = 0;
}

uint64_t SimpleMemory::mem_read(uint64_t size) {
  return nic_mem_read(size);
}

uint64_t SimpleMemory::mem_write(uint64_t size) {
  return nic_mem_write(size);
}

uint64_t SimpleMemory::npu_mem_read(uint64_t size) {
  npu_read_request_count++;
  return static_cast<uint64_t>(
      static_cast<double>(size) / npu_access_bw_GB);
}

uint64_t SimpleMemory::SimpleMemory::npu_mem_write(uint64_t size) {
  npu_write_request_count++;
  return static_cast<uint64_t>(
      static_cast<double>(size) / npu_access_bw_GB);
}

uint64_t SimpleMemory::nic_mem_read(uint64_t size) {
  nic_read_request_count++;
  timespec_t time = comm_NI->sim_get_time();
  double time_ns = time.time_val;
  double delay = (size / nic_access_bw_GB);
  double offset = 0;
  if (time_ns + access_latency < last_read_request_serviced) {
    offset = (last_read_request_serviced + delay) - time_ns;
    last_read_request_serviced += delay;
  } else {
    offset = (time_ns + access_latency + delay) - time_ns;
    last_read_request_serviced = time_ns + access_latency + delay;
  }
  return (uint64_t)(offset);
}

uint64_t SimpleMemory::nic_mem_write(uint64_t size) {
  nic_write_request_count++;
  timespec_t time = comm_NI->sim_get_time();
  double time_ns = time.time_val;
  double delay = (size / nic_access_bw_GB);
  double offset = 0;
  if (time_ns + access_latency < last_write_request_serviced) {
    offset = (last_write_request_serviced + delay) - time_ns;
    last_write_request_serviced += delay;
  } else {
    offset = (time_ns + access_latency + delay) - time_ns;
    last_write_request_serviced = time_ns + access_latency + delay;
  }
  return (uint64_t)(offset);
}

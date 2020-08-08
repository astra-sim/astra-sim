#include "SimpleMemory.hh"


SimpleMemory::SimpleMemory(AstraNetworkAPI *NI,double access_latency,double npu_access_bw_GB,double nic_access_bw_GB){
        this->last_request_serviced=0;
        this->NI=NI;
        this->access_latency=access_latency;
        this->npu_access_bw_GB=npu_access_bw_GB;
        this->nic_access_bw_GB=nic_access_bw_GB;
}
void SimpleMemory::set_network_api(AstraNetworkAPI *astraNetworkApi){
    this->NI=astraNetworkApi;
}
uint64_t SimpleMemory::npu_mem_read(uint64_t size){
    timespec_t time=NI->sim_get_time();
    double time_ns=time.time_val;
    double delay=access_latency+(size/npu_access_bw_GB);
    double max=last_request_serviced;
    if(max<time_ns){
        max=time_ns;
    }
    double offset=(max+delay)-time_ns;
    this->last_request_serviced=max+delay;
    return (uint64_t)(offset);

}
uint64_t SimpleMemory::SimpleMemory::npu_mem_write(uint64_t size){
    timespec_t time=NI->sim_get_time();
    double time_ns=time.time_val;
    double delay=access_latency+(size/npu_access_bw_GB);
    double max=last_request_serviced;
    if(max<time_ns){
        max=time_ns;
    }
    double offset=(max+delay)-time_ns;
    this->last_request_serviced=max+delay;
    return (uint64_t)(offset);
}
uint64_t SimpleMemory::nic_mem_read(uint64_t size){
    timespec_t time=NI->sim_get_time();
    double time_ns=time.time_val;
    double delay=access_latency+(size/nic_access_bw_GB);
    double max=last_request_serviced;
    if(max<time_ns){
        max=time_ns;
    }
    double offset=(max+delay)-time_ns;
    this->last_request_serviced=max+delay;
    return (uint64_t)(offset);
}
uint64_t SimpleMemory::nic_mem_write(uint64_t size){
    timespec_t time=NI->sim_get_time();
    double time_ns=time.time_val;
    double delay=access_latency+(size/nic_access_bw_GB);
    double max=last_request_serviced;
    if(max<time_ns){
        max=time_ns;
    }
    double offset=(max+delay)-time_ns;
    this->last_request_serviced=max+delay;
    return (uint64_t)(offset);
}
uint64_t SimpleMemory::mem_read(uint64_t size){
    return nic_mem_read(size);
}
uint64_t SimpleMemory::mem_write(uint64_t size){
    return nic_mem_write(size);
}
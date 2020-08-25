#include "SimpleMemory.hh"


SimpleMemory::SimpleMemory(AstraNetworkAPI *NI,double access_latency,double npu_access_bw_GB,double nic_access_bw_GB){
        this->last_request_serviced=0;
        this->read_request_count=0;
        this->write_request_count=0;
        this->NI=NI;
        this->access_latency=access_latency;
        this->npu_access_bw_GB=npu_access_bw_GB;
        this->nic_access_bw_GB=nic_access_bw_GB;
}
void SimpleMemory::set_network_api(AstraNetworkAPI *astraNetworkApi){
    this->NI=astraNetworkApi;
}
uint64_t SimpleMemory::npu_mem_read(uint64_t size){
    read_request_count++;
    //timespec_t time=NI->sim_get_time();
    //double time_ns=time.time_val;
    double delay=(size/npu_access_bw_GB);
    return delay;
    /*if(time_ns+access_latency<last_request_serviced){
        double offset=(last_request_serviced+delay)-time_ns;
        last_request_serviced+=delay;
        return (uint64_t)(offset);
    }
    else{
        double offset=(time_ns+access_latency+delay)-time_ns;
        last_request_serviced=time_ns+access_latency+delay;
        return (uint64_t)(offset);
    }*/
    //std::cout<<"npu read req, node id: "<<NI->sim_comm_get_rank()<<", bytes: "<<size<<", access latency: "<<(uint64_t)(offset)
             //<<", current delay: "<<delay<<", total requests: "<<request_count<<std::endl;

}
uint64_t SimpleMemory::SimpleMemory::npu_mem_write(uint64_t size){
    write_request_count++;
    //timespec_t time=NI->sim_get_time();
    //double time_ns=time.time_val;
    double delay=(size/npu_access_bw_GB);
    return delay;
    /*if(time_ns+access_latency<last_request_serviced){
        double offset=(last_request_serviced+delay)-time_ns;
        last_request_serviced+=delay;
        return (uint64_t)(offset);
    }
    else{
        double offset=(time_ns+access_latency+delay)-time_ns;
        last_request_serviced=time_ns+access_latency+delay;
        return (uint64_t)(offset);
    }*/
    //std::cout<<"npu write req, node id: "<<NI->sim_comm_get_rank()<<", bytes: "<<size<<", access latency: "<<(uint64_t)(offset)
             //<<", current delay: "<<delay<<", total requests: "<<request_count<<std::endl;
}
uint64_t SimpleMemory::nic_mem_read(uint64_t size){
    read_request_count++;
    timespec_t time=NI->sim_get_time();
    double time_ns=time.time_val;
    double delay=(size/nic_access_bw_GB);
    double offset=0;
    if(time_ns+access_latency<last_request_serviced){
        offset=(last_request_serviced+delay)-time_ns;
        last_request_serviced+=delay;
    }
    else{
        offset=(time_ns+access_latency+delay)-time_ns;
        last_request_serviced=time_ns+access_latency+delay;
    }
    //std::cout<<"nic read req,,node id: "<<NI->sim_comm_get_rank()<<", bytes: "<<size<<
    //", current delay: "<<delay<<", total read requests: "<<read_request_count<<std::endl;
    return (uint64_t)(offset);
}
uint64_t SimpleMemory::nic_mem_write(uint64_t size){
    write_request_count++;
    timespec_t time=NI->sim_get_time();
    double time_ns=time.time_val;
    double delay=(size/nic_access_bw_GB);
    double offset=0;
    if(time_ns+access_latency<last_request_serviced){
        offset=(last_request_serviced+delay)-time_ns;
        last_request_serviced+=delay;
    }
    else{
        offset=(time_ns+access_latency+delay)-time_ns;
        last_request_serviced=time_ns+access_latency+delay;
    }
    //std::cout<<"nic write req, node id: "<<NI->sim_comm_get_rank()<<", bytes: "<<size<<
    //", current delay: "<<delay<<", total write requests: "<<write_request_count<<std::endl;
    return (uint64_t)(offset);
}
uint64_t SimpleMemory::mem_read(uint64_t size){
    return nic_mem_read(size);
}
uint64_t SimpleMemory::mem_write(uint64_t size){
    return nic_mem_write(size);
}

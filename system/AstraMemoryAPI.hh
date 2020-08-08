#ifndef __ASTRAMEMORYAPI_HH__
#define __ASTRAMEMORYAPI_HH__
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdint>
#include "AstraNetworkAPI.hh"
struct timespec_t;
class AstraNetworkAPI;
class AstraMemoryAPI{
    public:
        virtual  uint64_t mem_read(uint64_t size)=0;
        virtual  uint64_t mem_write(uint64_t size)=0;
        virtual  uint64_t npu_mem_read(uint64_t size)=0;
        virtual  uint64_t npu_mem_write(uint64_t size)=0;
        virtual  uint64_t nic_mem_read(uint64_t size)=0;
        virtual  uint64_t nic_mem_write(uint64_t size)=0;
        virtual ~AstraMemoryAPI()= default;
};
class SimpleMemory: public AstraMemoryAPI{
    public:
        double last_request_serviced;
        double npu_access_bw_GB;
        double nic_access_bw_GB;
        double access_latency;
        AstraNetworkAPI *NI;
        SimpleMemory(AstraNetworkAPI *NI,double access_latency,double npu_access_bw_GB,double nic_access_bw_GB){
            this->last_request_serviced=0;
            this->NI=NI;
            this->access_latency=access_latency;
            this->npu_access_bw_GB=npu_access_bw_GB;
            this->nic_access_bw_GB=nic_access_bw_GB;
        }
        void set_network_api(AstraNetworkAPI *astraNetworkApi){
            this->NI=astraNetworkApi;
        }
        uint64_t npu_mem_read(uint64_t size){
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
        uint64_t npu_mem_write(uint64_t size){
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
        uint64_t nic_mem_read(uint64_t size){
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
        uint64_t nic_mem_write(uint64_t size){
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
        uint64_t mem_read(uint64_t size){
            return nic_mem_read(size);
        }
        uint64_t mem_write(uint64_t size){
            return nic_mem_write(size);
        }
};
#endif

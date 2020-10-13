/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __ASTRANETWORKAPI_HH__
#define __ASTRANETWORKAPI_HH__
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include "AstraMemoryAPI.hh"
#include "AstraSimDataAPI.hh"
namespace AstraSim{
struct sim_comm {
    std::string comm_name;
};

enum time_type_e {SE, MS, US, NS, FS};

struct timespec_t {
    time_type_e time_res;
    double time_val;
};
enum req_type_e {UINT8, BFLOAT16, FP32};
struct sim_request {
    uint32_t srcRank;
    uint32_t dstRank;
    uint32_t tag;
    req_type_e reqType;
    uint32_t reqCount;
    uint32_t vnet;
    uint32_t layerNum;
};

class MetaData {
public:
    timespec_t timestamp;
};

class AstraNetworkAPI{
public:
    bool enabled;
    int rank;

    virtual int sim_comm_size(sim_comm comm, int* size)=0;
    virtual int sim_comm_get_rank(){return rank;};
    virtual int sim_comm_set_rank(int rank){this->rank=rank;return this->rank;};
    virtual int sim_finish()=0;
    virtual double sim_time_resolution()=0;
    virtual int sim_init(AstraMemoryAPI* MEM)=0;
    virtual timespec_t sim_get_time()=0;
    virtual void sim_schedule(timespec_t delta, void (*fun_ptr)(void *fun_arg), void *fun_arg)=0;
    virtual int sim_send(void *buffer, int count, int type, int dst, int tag, sim_request *request, void (*msg_handler)(void *fun_arg), void* fun_arg)=0;
    virtual int sim_recv(void *buffer, int count, int type, int src, int tag, sim_request *request, void (*msg_handler)(void *fun_arg), void* fun_arg)=0;
    virtual void pass_front_end_report(AstraSimDataAPI astraSimDataAPI){return;};

    AstraNetworkAPI(int rank){this->rank=rank;enabled= true;};
    virtual ~AstraNetworkAPI() {}; // ADDED BY PALLAVI
};
}
#endif

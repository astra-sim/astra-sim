#ifndef __COMMONAPI_HH__
#define __COMMONAPI_HH__
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include "MemoryCommonAPI.hh"
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

class CommonAPI{
public:
    virtual int sim_comm_size(sim_comm comm, int* size)=0;
    virtual int sim_comm_get_rank(sim_comm comm, int *size)=0;
    virtual int sim_comm_set_rank(sim_comm comm, int rank)=0;
    virtual int sim_finish()=0;
    virtual double sim_time_resolution()=0;
    virtual int sim_init(MemoryCommonAPI* MEM)=0;
    virtual timespec_t sim_get_time()=0;
    virtual void sim_schedule(timespec_t delta, void (*fun_ptr)(void *fun_arg), void *fun_arg)=0;
    virtual int sim_send(void *buffer, int count, int type, int dst, int tag, sim_request *request, void (*msg_handler)(void *fun_arg), void* fun_arg)=0;
    virtual int sim_recv(void *buffer, int count, int type, int src, int tag, sim_request *request, void (*msg_handler)(void *fun_arg), void* fun_arg)=0;

    //compatibility reason, do not do anything, should be removed later
    bool enabled;
    bool testFreeVC(int vnet){return true;};
    bool acquireFreeVC(int vnet){return true;};
    bool remove_packet_from_waiting_list_done(int vnet){return true;};
    bool remove_packet_from_waiting_list(int vnet){return true;};
    CommonAPI(){enabled= true;};
    virtual ~CommonAPI() {}; // ADDED BY PALLAVI
};
#endif
